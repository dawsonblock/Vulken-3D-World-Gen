#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>

namespace voxelvk {
    
    class MetricValue {
    public:
        virtual ~MetricValue() = default;
        virtual std::string serialize() const = 0;
        virtual void reset() = 0;
    };
    
    class Counter : public MetricValue {
    private:
        std::atomic<uint64_t> value_{0};
        
    public:
        void increment(uint64_t delta = 1) {
            value_.fetch_add(delta, std::memory_order_relaxed);
        }
        
        uint64_t get() const {
            return value_.load(std::memory_order_relaxed);
        }
        
        std::string serialize() const override {
            return std::to_string(get());
        }
        
        void reset() override {
            value_.store(0, std::memory_order_relaxed);
        }
    };
    
    class Gauge : public MetricValue {
    private:
        std::atomic<double> value_{0.0};
        
    public:
        void set(double val) {
            value_.store(val, std::memory_order_relaxed);
        }
        
        void increment(double delta = 1.0) {
            double current = value_.load(std::memory_order_relaxed);
            while (!value_.compare_exchange_weak(current, current + delta, std::memory_order_relaxed)) {
                // Retry on failure
            }
        }
        
        void decrement(double delta = 1.0) {
            increment(-delta);
        }
        
        double get() const {
            return value_.load(std::memory_order_relaxed);
        }
        
        std::string serialize() const override {
            return std::to_string(get());
        }
        
        void reset() override {
            value_.store(0.0, std::memory_order_relaxed);
        }
    };
    
    class Histogram : public MetricValue {
    private:
        mutable std::mutex mutex_;
        std::vector<double> buckets_;
        std::vector<uint64_t> counts_;
        uint64_t totalCount_{0};
        double sum_{0.0};
        
    public:
        Histogram(const std::vector<double>& buckets = {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0})
            : buckets_(buckets), counts_(buckets.size() + 1, 0) {
            std::sort(buckets_.begin(), buckets_.end());
        }
        
        void observe(double value) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            sum_ += value;
            totalCount_++;
            
            // Find appropriate bucket
            size_t bucketIndex = 0;
            for (size_t i = 0; i < buckets_.size(); ++i) {
                if (value <= buckets_[i]) {
                    bucketIndex = i;
                    break;
                }
                bucketIndex = buckets_.size(); // +Inf bucket
            }
            
            // Increment all buckets from the found bucket to the end (cumulative)
            for (size_t i = bucketIndex; i < counts_.size(); ++i) {
                counts_[i]++;
            }
        }
        
        std::string serialize() const override {
            std::lock_guard<std::mutex> lock(mutex_);
            std::ostringstream oss;
            
            // Bucket counts
            for (size_t i = 0; i < buckets_.size(); ++i) {
                oss << "le=\"" << buckets_[i] << "\" " << counts_[i] << "\n";
            }
            oss << "le=\"+Inf\" " << counts_.back() << "\n";
            
            // Summary stats
            oss << "_sum " << sum_ << "\n";
            oss << "_count " << totalCount_;
            
            return oss.str();
        }
        
        void reset() override {
            std::lock_guard<std::mutex> lock(mutex_);
            std::fill(counts_.begin(), counts_.end(), 0);
            totalCount_ = 0;
            sum_ = 0.0;
        }
        
        double getSum() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return sum_;
        }
        
        uint64_t getCount() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return totalCount_;
        }
        
        double getAverage() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return totalCount_ > 0 ? sum_ / totalCount_ : 0.0;
        }
    };
    
    class MetricsRegistry {
    private:
        mutable std::mutex mutex_;
        std::map<std::string, std::shared_ptr<MetricValue>> metrics_;
        std::map<std::string, std::map<std::string, std::string>> labels_;
        static std::unique_ptr<MetricsRegistry> instance_;
        
        MetricsRegistry() = default;
        
    public:
        static MetricsRegistry& getInstance() {
            if (!instance_) {
                instance_ = std::unique_ptr<MetricsRegistry>(new MetricsRegistry());
            }
            return *instance_;
        }
        
        std::shared_ptr<Counter> createCounter(const std::string& name, const std::string& help = "") {
            std::lock_guard<std::mutex> lock(mutex_);
            auto counter = std::make_shared<Counter>();
            metrics_[name] = counter;
            return counter;
        }
        
        std::shared_ptr<Gauge> createGauge(const std::string& name, const std::string& help = "") {
            std::lock_guard<std::mutex> lock(mutex_);
            auto gauge = std::make_shared<Gauge>();
            metrics_[name] = gauge;
            return gauge;
        }
        
        std::shared_ptr<Histogram> createHistogram(const std::string& name, const std::string& help = "", 
                                                 const std::vector<double>& buckets = {}) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto histogram = buckets.empty() ? std::make_shared<Histogram>() : std::make_shared<Histogram>(buckets);
            metrics_[name] = histogram;
            return histogram;
        }
        
        void addLabel(const std::string& metricName, const std::string& key, const std::string& value) {
            std::lock_guard<std::mutex> lock(mutex_);
            labels_[metricName][key] = value;
        }
        
        std::string exportMetrics() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::ostringstream oss;
            
            for (const auto& [name, metric] : metrics_) {
                oss << "# HELP " << name << " VoxelVK metric\n";
                oss << "# TYPE " << name << " ";
                
                // Determine type
                if (std::dynamic_pointer_cast<Counter>(metric)) {
                    oss << "counter\n";
                } else if (std::dynamic_pointer_cast<Gauge>(metric)) {
                    oss << "gauge\n";
                } else if (std::dynamic_pointer_cast<Histogram>(metric)) {
                    oss << "histogram\n";
                }
                
                // Add labels if any
                std::string labelStr;
                auto labelIt = labels_.find(name);
                if (labelIt != labels_.end() && !labelIt->second.empty()) {
                    labelStr = "{";
                    bool first = true;
                    for (const auto& [key, value] : labelIt->second) {
                        if (!first) labelStr += ",";
                        labelStr += key + "=\"" + value + "\"";
                        first = false;
                    }
                    labelStr += "}";
                }
                
                // Serialize metric value
                std::string value = metric->serialize();
                if (std::dynamic_pointer_cast<Histogram>(metric)) {
                    // Histogram has special formatting
                    std::istringstream iss(value);
                    std::string line;
                    while (std::getline(iss, line)) {
                        if (line.find("_sum") != std::string::npos || line.find("_count") != std::string::npos) {
                            oss << name << line.substr(line.find("_")) << labelStr << " " << line.substr(line.find(" ") + 1) << "\n";
                        } else {
                            oss << name << "_bucket{" << line << "}" << labelStr.substr(1) << "\n"; // Remove first {
                        }
                    }
                } else {
                    oss << name << labelStr << " " << value << "\n";
                }
                
                oss << "\n";
            }
            
            return oss.str();
        }
        
        void reset() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [name, metric] : metrics_) {
                metric->reset();
            }
        }
    };
    
    class MetricsExporter {
    private:
        bool enabled_;
        std::string endpoint_;
        uint16_t port_;
        std::thread exportThread_;
        std::atomic<bool> running_{false};
        
    public:
        MetricsExporter(bool enabled = false, const std::string& endpoint = "localhost", uint16_t port = 9464)
            : enabled_(enabled), endpoint_(endpoint), port_(port) {}
        
        ~MetricsExporter() {
            stop();
        }
        
        void start() {
            if (!enabled_ || running_.load()) return;
            
            running_.store(true);
            exportThread_ = std::thread([this]() {
                while (running_.load()) {
                    // In a real implementation, this would serve HTTP requests
                    // For now, we'll just export to stdout periodically
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    
                    if (running_.load()) {
                        auto metrics = MetricsRegistry::getInstance().exportMetrics();
                        if (!metrics.empty()) {
                            printf("=== METRICS EXPORT ===\n%s======================\n", metrics.c_str());
                            fflush(stdout);
                        }
                    }
                }
            });
        }
        
        void stop() {
            if (running_.load()) {
                running_.store(false);
                if (exportThread_.joinable()) {
                    exportThread_.join();
                }
            }
        }
        
        std::string getMetrics() const {
            return MetricsRegistry::getInstance().exportMetrics();
        }
    };
    
    // Global convenience functions
    inline MetricsRegistry& Metrics() {
        return MetricsRegistry::getInstance();
    }
    
} // namespace voxelvk