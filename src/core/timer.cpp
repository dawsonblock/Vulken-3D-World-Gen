#include "timer.hpp"
#include "logger.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace voxelvk {

// ScopedTimer implementation
ScopedTimer::ScopedTimer(const std::string& name) 
    : m_name(name), m_callback(nullptr) {
}

ScopedTimer::ScopedTimer(const std::string& name, std::function<void(double)> callback)
    : m_name(name), m_callback(callback) {
}

ScopedTimer::~ScopedTimer() {
    double elapsed = m_timer.ElapsedMs();
    
    if (m_callback) {
        m_callback(elapsed);
    } else {
        VXL_DEBUG("Timer [{}]: {:.3f}ms", m_name, elapsed);
    }
}

// Profiler implementation
Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::BeginSection(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active_timers[name] = Timer();
}

void Profiler::EndSection(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_active_timers.find(name);
    if (it != m_active_timers.end()) {
        double elapsed = it->second.ElapsedMs();
        m_stats[name].Update(elapsed);
        m_active_timers.erase(it);
    }
}

void Profiler::RecordTime(const std::string& name, double time_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats[name].Update(time_ms);
}

const Profiler::Stats& Profiler::GetStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    static Stats empty_stats;
    auto it = m_stats.find(name);
    return (it != m_stats.end()) ? it->second : empty_stats;
}

std::vector<std::pair<std::string, Profiler::Stats>> Profiler::GetAllStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<std::string, Stats>> result;
    result.reserve(m_stats.size());
    
    for (const auto& pair : m_stats) {
        result.emplace_back(pair);
    }
    
    // Sort by total time (descending)
    std::sort(result.begin(), result.end(), 
        [](const auto& a, const auto& b) {
            return a.second.total_ms > b.second.total_ms;
        });
    
    return result;
}

void Profiler::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.clear();
    m_active_timers.clear();
}

void Profiler::Reset(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end()) {
        it->second.Reset();
    }
}

void Profiler::PrintSummary() const {
    PrintSummary("");
}

void Profiler::PrintSummary(const std::string& filter) const {
    auto stats = GetAllStats();
    
    if (stats.empty()) {
        VXL_INFO("Profiler: No timing data available");
        return;
    }
    
    std::stringstream ss;
    ss << "\n=== Profiler Summary ===\n";
    ss << std::left << std::setw(30) << "Section"
       << std::right << std::setw(10) << "Count"
       << std::setw(12) << "Total(ms)"
       << std::setw(12) << "Avg(ms)"
       << std::setw(12) << "Min(ms)"
       << std::setw(12) << "Max(ms)" << "\n";
    ss << std::string(88, '-') << "\n";
    
    for (const auto& [name, stat] : stats) {
        if (!filter.empty() && name.find(filter) == std::string::npos) {
            continue;
        }
        
        ss << std::left << std::setw(30) << name
           << std::right << std::setw(10) << stat.count
           << std::setw(12) << std::fixed << std::setprecision(3) << stat.total_ms
           << std::setw(12) << std::fixed << std::setprecision(3) << stat.avg_ms
           << std::setw(12) << std::fixed << std::setprecision(3) << stat.min_ms
           << std::setw(12) << std::fixed << std::setprecision(3) << stat.max_ms << "\n";
    }
    
    ss << "========================\n";
    VXL_INFO("{}", ss.str());
}

// FrameRateCounter implementation
FrameRateCounter::FrameRateCounter(size_t window_size)
    : m_window_size(window_size)
    , m_current_index(0)
    , m_fps(0.0)
    , m_frame_time_ms(0.0)
    , m_first_frame(true) {
    m_frame_times.resize(m_window_size, 0.0);
}

void FrameRateCounter::Update() {
    if (m_first_frame) {
        m_frame_timer.Reset();
        m_first_frame = false;
        return;
    }
    
    m_frame_time_ms = m_frame_timer.ElapsedMs();
    m_frame_timer.Reset();
    
    // Store frame time in circular buffer
    m_frame_times[m_current_index] = m_frame_time_ms;
    m_current_index = (m_current_index + 1) % m_window_size;
    
    // Calculate average FPS over window
    double total_time = 0.0;
    size_t valid_samples = 0;
    
    for (double frame_time : m_frame_times) {
        if (frame_time > 0.0) {
            total_time += frame_time;
            valid_samples++;
        }
    }
    
    if (valid_samples > 0 && total_time > 0.0) {
        double avg_frame_time = total_time / valid_samples;
        m_fps = 1000.0 / avg_frame_time; // Convert ms to FPS
    }
}

double FrameRateCounter::GetAvgFrameTimeMs() const {
    double total_time = 0.0;
    size_t valid_samples = 0;
    
    for (double frame_time : m_frame_times) {
        if (frame_time > 0.0) {
            total_time += frame_time;
            valid_samples++;
        }
    }
    
    return (valid_samples > 0) ? (total_time / valid_samples) : 0.0;
}

void FrameRateCounter::Reset() {
    std::fill(m_frame_times.begin(), m_frame_times.end(), 0.0);
    m_current_index = 0;
    m_fps = 0.0;
    m_frame_time_ms = 0.0;
    m_first_frame = true;
}

} // namespace voxelvk