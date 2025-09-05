#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/fmt.h>
#include <memory>
#include <string>
#include <map>
#include <chrono>

namespace voxelvk {
    
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5
    };
    
    class StructuredLogger {
    private:
        std::shared_ptr<spdlog::logger> logger_;
        static std::unique_ptr<StructuredLogger> instance_;
        
        StructuredLogger() = default;
        
    public:
        static StructuredLogger& getInstance() {
            if (!instance_) {
                instance_ = std::unique_ptr<StructuredLogger>(new StructuredLogger());
                instance_->initialize();
            }
            return *instance_;
        }
        
        void initialize() {
            // Get log level from environment variable
            std::string logLevelStr = getenv("LOG_LEVEL") ? getenv("LOG_LEVEL") : "info";
            spdlog::level::level_enum level = spdlog::level::info;
            
            if (logLevelStr == "trace") level = spdlog::level::trace;
            else if (logLevelStr == "debug") level = spdlog::level::debug;
            else if (logLevelStr == "info") level = spdlog::level::info;
            else if (logLevelStr == "warn") level = spdlog::level::warn;
            else if (logLevelStr == "error") level = spdlog::level::err;
            else if (logLevelStr == "critical") level = spdlog::level::critical;
            
            // Create sinks
            std::vector<spdlog::sink_ptr> sinks;
            
            // Console sink with color
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(level);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            sinks.push_back(console_sink);
            
            // File sink with rotation
            std::string logDir = "logs";
            std::filesystem::create_directories(logDir);
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logDir + "/voxelvk.log", 1024*1024*5, 3); // 5MB, 3 files
            file_sink->set_level(spdlog::level::trace); // File gets all levels
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
            
            // Create logger
            logger_ = std::make_shared<spdlog::logger>("voxelvk", sinks.begin(), sinks.end());
            logger_->set_level(level);
            logger_->flush_on(spdlog::level::warn);
            
            // Register as default logger
            spdlog::set_default_logger(logger_);
        }
        
        template<typename... Args>
        void trace(const std::string& msg, Args&&... args) {
            logger_->trace(msg, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void debug(const std::string& msg, Args&&... args) {
            logger_->debug(msg, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void info(const std::string& msg, Args&&... args) {
            logger_->info(msg, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void warn(const std::string& msg, Args&&... args) {
            logger_->warn(msg, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void error(const std::string& msg, Args&&... args) {
            logger_->error(msg, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void critical(const std::string& msg, Args&&... args) {
            logger_->critical(msg, std::forward<Args>(args)...);
        }
        
        // Structured logging with JSON-like fields
        class LogEntry {
        private:
            std::map<std::string, std::string> fields_;
            spdlog::level::level_enum level_;
            StructuredLogger& logger_;
            
        public:
            LogEntry(StructuredLogger& logger, spdlog::level::level_enum level) 
                : logger_(logger), level_(level) {}
            
            LogEntry& field(const std::string& key, const std::string& value) {
                fields_[key] = value;
                return *this;
            }
            
            LogEntry& field(const std::string& key, int value) {
                fields_[key] = std::to_string(value);
                return *this;
            }
            
            LogEntry& field(const std::string& key, float value) {
                fields_[key] = fmt::format("{:.3f}", value);
                return *this;
            }
            
            LogEntry& field(const std::string& key, double value) {
                fields_[key] = fmt::format("{:.6f}", value);
                return *this;
            }
            
            void msg(const std::string& message) {
                std::string structured_msg = message;
                for (const auto& [key, value] : fields_) {
                    structured_msg += fmt::format(" {}={}", key, value);
                }
                
                switch (level_) {
                    case spdlog::level::trace:
                        logger_.logger_->trace(structured_msg);
                        break;
                    case spdlog::level::debug:
                        logger_.logger_->debug(structured_msg);
                        break;
                    case spdlog::level::info:
                        logger_.logger_->info(structured_msg);
                        break;
                    case spdlog::level::warn:
                        logger_.logger_->warn(structured_msg);
                        break;
                    case spdlog::level::err:
                        logger_.logger_->error(structured_msg);
                        break;
                    case spdlog::level::critical:
                        logger_.logger_->critical(structured_msg);
                        break;
                    default:
                        logger_.logger_->info(structured_msg);
                        break;
                }
            }
        };
        
        LogEntry trace() { return LogEntry(*this, spdlog::level::trace); }
        LogEntry debug() { return LogEntry(*this, spdlog::level::debug); }
        LogEntry info() { return LogEntry(*this, spdlog::level::info); }
        LogEntry warn() { return LogEntry(*this, spdlog::level::warn); }
        LogEntry error() { return LogEntry(*this, spdlog::level::err); }
        LogEntry critical() { return LogEntry(*this, spdlog::level::critical); }
        
        void flush() {
            logger_->flush();
        }
    };
    
    // Global convenience functions
    inline StructuredLogger& Log() {
        return StructuredLogger::getInstance();
    }
    
} // namespace voxelvk

// Convenience macros
#define VOXEL_TRACE(...) voxelvk::Log().trace(__VA_ARGS__)
#define VOXEL_DEBUG(...) voxelvk::Log().debug(__VA_ARGS__)
#define VOXEL_INFO(...) voxelvk::Log().info(__VA_ARGS__)
#define VOXEL_WARN(...) voxelvk::Log().warn(__VA_ARGS__)
#define VOXEL_ERROR(...) voxelvk::Log().error(__VA_ARGS__)
#define VOXEL_CRITICAL(...) voxelvk::Log().critical(__VA_ARGS__)

// Structured logging macros
#define VOXEL_LOG_TRACE() voxelvk::Log().trace()
#define VOXEL_LOG_DEBUG() voxelvk::Log().debug()
#define VOXEL_LOG_INFO() voxelvk::Log().info()
#define VOXEL_LOG_WARN() voxelvk::Log().warn()
#define VOXEL_LOG_ERROR() voxelvk::Log().error()
#define VOXEL_LOG_CRITICAL() voxelvk::Log().critical()