#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

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
    explicit ScopedTimer(const std::string& name);
    ScopedTimer(const std::string& name, std::function<void(double)> callback);
    ~ScopedTimer();
    
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
    
    static Profiler& Instance();
    
    void BeginSection(const std::string& name);
    void EndSection(const std::string& name);
    void RecordTime(const std::string& name, double time_ms);
    
    const Stats& GetStats(const std::string& name) const;
    std::vector<std::pair<std::string, Stats>> GetAllStats() const;
    
    void Reset();
    void Reset(const std::string& name);
    
    // Print summary to log
    void PrintSummary() const;
    void PrintSummary(const std::string& filter) const;
    
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
    FrameRateCounter(size_t window_size = 60);
    
    void Update();
    
    double GetFPS() const { return m_fps; }
    double GetFrameTimeMs() const { return m_frame_time_ms; }
    double GetAvgFrameTimeMs() const;
    
    void Reset();
    
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