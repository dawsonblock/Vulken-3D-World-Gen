#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <mutex>
#include <fstream>
#include <chrono>
#include <iomanip>

namespace voxelvk {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

class Logger {
public:
    explicit Logger(const std::string& name);
    ~Logger();
    
    // Template methods for formatted logging
    template<typename... Args>
    void Trace(const std::string& format, Args&&... args) {
        Log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Debug(const std::string& format, Args&&... args) {
        Log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Info(const std::string& format, Args&&... args) {
        Log(LogLevel::INFO, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Warn(const std::string& format, Args&&... args) {
        Log(LogLevel::WARN, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Error(const std::string& format, Args&&... args) {
        Log(LogLevel::ERROR, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Fatal(const std::string& format, Args&&... args) {
        Log(LogLevel::FATAL, format, std::forward<Args>(args)...);
    }
    
    // Set global log level
    static void SetGlobalLogLevel(LogLevel level);
    static void SetLogFile(const std::string& filename);
    static void EnableConsoleOutput(bool enable);
    
private:
    std::string name_;
    static LogLevel global_log_level_;
    static std::ofstream log_file_;
    static bool console_output_enabled_;
    static std::mutex log_mutex_;
    
    template<typename... Args>
    void Log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < global_log_level_) {
            return;
        }
        
        std::string formatted_message;
        if constexpr (sizeof...(args) == 0) {
            formatted_message = format;
        } else {
            formatted_message = FormatString(format, std::forward<Args>(args)...);
        }
        WriteLog(level, formatted_message);
    }
    
    template<typename T>
    std::string FormatString(const std::string& format, T&& value) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            std::ostringstream oss;
            oss << value;
            std::string result = format;
            result.replace(pos, 2, oss.str());
            return result;
        }
        return format;
    }
    
    template<typename T, typename... Args>
    std::string FormatString(const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            std::ostringstream oss;
            oss << value;
            std::string partial = format;
            partial.replace(pos, 2, oss.str());
            return FormatString(partial, std::forward<Args>(args)...);
        }
        return FormatString(format, std::forward<Args>(args)...);
    }
    
    void WriteLog(LogLevel level, const std::string& message);
    std::string GetTimestamp();
    std::string LogLevelToString(LogLevel level);
};

} // namespace voxelvk

// Legacy logging macro shims
#ifndef VXL_TRACE
#define VXL_TRACE(fmt, ...) do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Trace(fmt, ##__VA_ARGS__); } while(0)
#endif
#ifndef VXL_DEBUG
#define VXL_DEBUG(fmt, ...) do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Debug(fmt, ##__VA_ARGS__); } while(0)
#endif
#ifndef VXL_INFO
#define VXL_INFO(fmt, ...)  do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Info(fmt, ##__VA_ARGS__); } while(0)
#endif
#ifndef VXL_WARN
#define VXL_WARN(fmt, ...)  do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Warn(fmt, ##__VA_ARGS__); } while(0)
#endif
#ifndef VXL_ERROR
#define VXL_ERROR(fmt, ...) do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Error(fmt, ##__VA_ARGS__); } while(0)
#endif
#ifndef VXL_FATAL
#define VXL_FATAL(fmt, ...) do { static ::voxelvk::Logger _vxl_logger__("App"); _vxl_logger__.Fatal(fmt, ##__VA_ARGS__); } while(0)
#endif