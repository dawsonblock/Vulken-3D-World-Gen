#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <algorithm>   // std::min, std::max
#include <limits>      // std::numeric_limits
#include <utility>     // std::pair
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>    // std::cout

namespace voxelvk {

// High-resolution timer for performance measurement
class Timer {
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::duration<double, std::milli>;
    
    Timer() : m_start(std::chrono::high_resolution_clock::now()) {}
    
    // Reset the timer
    void Reset() {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    // Get elapsed time in milliseconds
    double ElapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - m_start).count();
    }
    
    // Get elapsed time in seconds
    double ElapsedSec() const {
        return ElapsedMs() / 1000.0;
    }
    
    // Get elapsed time in microseconds
    double ElapsedUs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(now - m_start).count();
    }
    
private:
    TimePoint m_start;
};

// RAII timer for scope-based timing
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name)
        : m_name(name) {}
    ScopedTimer(const std::string& name, std::function<void(double)> callback)
        : m_name(name), m_callback(std::move(callback)) {}
    ~ScopedTimer() {
        double ms = m_timer.ElapsedMs();
        if (m_callback) {
            m_callback(ms);
        } else {
            // Default: log concise line
            std::cout << "[Timer] " << m_name << ": " << ms << " ms\n";
        }
    }
    
    double ElapsedMs() const { return m_timer.ElapsedMs(); }
    
private:
    Timer m_timer;
    std::string m_name;
    std::function<void(double)> m_callback;
};

// Profiler for collecting timing statistics
class Profiler {
public:
    struct Stats {
        double total_ms = 0.0;
        double min_ms = std::numeric_limits<double>::max();
        double max_ms = 0.0;
        double avg_ms = 0.0;
        size_t count = 0;
        
        void Update(double time_ms) {
            total_ms += time_ms;
            min_ms = std::min(min_ms, time_ms);
            max_ms = std::max(max_ms, time_ms);
            count++;
            avg_ms = total_ms / count;
        }
        
        void Reset() {
            total_ms = 0.0;
            min_ms = std::numeric_limits<double>::max();
            max_ms = 0.0;
            avg_ms = 0.0;
            count = 0;
        }
    };
    
    static Profiler& Instance() {
        static Profiler inst;
        return inst;
    }
    
    void BeginSection(const std::string& name) {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_active_timers[name] = Timer{};
    }
    void EndSection(const std::string& name) {
        std::lock_guard<std::mutex> lg(m_mutex);
        auto it = m_active_timers.find(name);
        if (it == m_active_timers.end()) return;
        double ms = it->second.ElapsedMs();
        m_active_timers.erase(it);
        m_stats[name].Update(ms);
    }
    void RecordTime(const std::string& name, double time_ms) {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_stats[name].Update(time_ms);
    }
    
    const Stats& GetStats(const std::string& name) const {
        std::lock_guard<std::mutex> lg(m_mutex);
        auto it = m_stats.find(name);
        if (it != m_stats.end()) return it->second;
        static Stats empty{};
        return empty;
    }
    std::vector<std::pair<std::string, Stats>> GetAllStats() const {
        std::lock_guard<std::mutex> lg(m_mutex);
        std::vector<std::pair<std::string, Stats>> out;
        out.reserve(m_stats.size());
        for (const auto& kv : m_stats) out.emplace_back(kv.first, kv.second);
        return out;
    }
    
    void Reset() {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_stats.clear();
        m_active_timers.clear();
    }
    void Reset(const std::string& name) {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_stats.erase(name);
        m_active_timers.erase(name);
    }
    
    // Print summary to log
    void PrintSummary() const {
        auto all = GetAllStats();
        std::cout << "=== Profiler Summary ===\n";
        for (const auto& kv : all) {
            const auto& n = kv.first;
            const auto& s = kv.second;
            if (s.count == 0) continue;
            std::cout << n << ": count=" << s.count
                      << " avg=" << s.avg_ms << "ms"
                      << " min=" << s.min_ms << "ms"
                      << " max=" << s.max_ms << "ms"
                      << " total=" << s.total_ms << "ms\n";
        }
    }
    void PrintSummary(const std::string& filter) const {
        auto all = GetAllStats();
        std::cout << "=== Profiler Summary (filter: " << filter << ") ===\n";
        for (const auto& kv : all) {
            if (kv.first.find(filter) == std::string::npos) continue;
            const auto& n = kv.first;
            const auto& s = kv.second;
            if (s.count == 0) continue;
            std::cout << n << ": count=" << s.count
                      << " avg=" << s.avg_ms << "ms"
                      << " min=" << s.min_ms << "ms"
                      << " max=" << s.max_ms << "ms"
                      << " total=" << s.total_ms << "ms\n";
        }
    }
    
private:
    std::unordered_map<std::string, Stats> m_stats;
    std::unordered_map<std::string, Timer> m_active_timers;
    mutable std::mutex m_mutex;
};

// Helper class for automatic profiling
class ProfilerScope {
public:
    explicit ProfilerScope(const std::string& name) : m_name(name) {
        Profiler::Instance().BeginSection(m_name);
    }
    
    ~ProfilerScope() {
        Profiler::Instance().EndSection(m_name);
    }
    
private:
    std::string m_name;
};

// Macros for convenient timing
#define VXL_TIMER(name) ::voxelvk::ScopedTimer timer_##__LINE__(name)
#define VXL_PROFILE(name) ::voxelvk::ProfilerScope profile_##__LINE__(name)
#define VXL_PROFILE_FUNCTION() VXL_PROFILE(__FUNCTION__)

// Frame rate counter
class FrameRateCounter {
public:
    FrameRateCounter(size_t window_size = 60)
        : m_frame_times(window_size, 0.0),
          m_window_size(window_size),
          m_current_index(0),
          m_fps(0.0),
          m_frame_time_ms(0.0),
          m_first_frame(true) {}

    void Update() {
        double elapsed = m_frame_timer.ElapsedMs();
        m_frame_timer.Reset();
        if (m_first_frame) {
            m_first_frame = false;
            return; // skip first frame (no delta)
        }
        m_frame_time_ms = elapsed;
        m_fps = (elapsed > 0.0) ? (1000.0 / elapsed) : 0.0;
        m_frame_times[m_current_index] = elapsed;
        m_current_index = (m_current_index + 1) % m_window_size;
    }
    
    double GetFPS() const { return m_fps; }
    double GetFrameTimeMs() const { return m_frame_time_ms; }
    double GetAvgFrameTimeMs() const {
        size_t count = 0;
        double sum = 0.0;
        for (double v : m_frame_times) {
            if (v > 0.0) { sum += v; ++count; }
        }
        return (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
    }
    
    void Reset() {
        std::fill(m_frame_times.begin(), m_frame_times.end(), 0.0);
        m_current_index = 0;
        m_fps = 0.0;
        m_frame_time_ms = 0.0;
        m_first_frame = true;
        m_frame_timer.Reset();
    }
    
private:
    std::vector<double> m_frame_times;
    size_t m_window_size;
    size_t m_current_index;
    Timer m_frame_timer;
    double m_fps;
    double m_frame_time_ms;
    bool m_first_frame;
};

} // namespace voxelvk