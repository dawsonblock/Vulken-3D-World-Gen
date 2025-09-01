#include "logger.hpp"
#include <iostream>
#include <iomanip>

namespace voxelvk {

// Static member definitions
LogLevel Logger::global_log_level_ = LogLevel::INFO;
std::ofstream Logger::log_file_;
bool Logger::console_output_enabled_ = true;
std::mutex Logger::log_mutex_;

Logger::Logger(const std::string& name) : name_(name) {}

Logger::~Logger() = default;

void Logger::SetGlobalLogLevel(LogLevel level) {
    global_log_level_ = level;
}

void Logger::SetLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    log_file_.open(filename, std::ios::app);
}

void Logger::EnableConsoleOutput(bool enable) {
    console_output_enabled_ = enable;
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string log_entry = "[" + GetTimestamp() + "] [" + LogLevelToString(level) + "] [" + name_ + "] " + message;
    
    if (console_output_enabled_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << log_entry << std::endl;
        } else {
            std::cout << log_entry << std::endl;
        }
    }
    
    if (log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    }
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

} // namespace voxelvk