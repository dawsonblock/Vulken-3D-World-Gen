#pragma once

#include <string>
#include <functional>
#include <vector>
#include <fstream>
#include <chrono>
#include <ctime>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#else
#include <execinfo.h>
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace voxelvk {
    
    struct CrashInfo {
        int signal = 0;
        std::string signalName;
        std::string timestamp;
        std::string executable;
        std::string workingDirectory;
        std::vector<std::string> stackTrace;
        std::string systemInfo;
        std::string additionalInfo;
    };
    
    class CrashHandler {
    private:
        static CrashHandler* instance_;
        std::vector<std::function<void(const CrashInfo&)>> callbacks_;
        std::string crashLogPath_;
        bool initialized_ = false;
        
        CrashHandler() = default;
        
        static void signalHandler(int signal) {
            if (instance_) {
                instance_->handleCrash(signal);
            }
        }
        
#ifdef _WIN32
        static LONG WINAPI exceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
            if (instance_) {
                instance_->handleException(exceptionInfo);
            }
            return EXCEPTION_EXECUTE_HANDLER;
        }
#endif
        
        void handleCrash(int signal) {
            CrashInfo info;
            info.signal = signal;
            info.signalName = getSignalName(signal);
            info.timestamp = getCurrentTimestamp();
            info.executable = getExecutablePath();
            info.workingDirectory = getCurrentWorkingDirectory();
            info.stackTrace = getStackTrace();
            info.systemInfo = getSystemInfo();
            
            // Write crash log
            writeCrashLog(info);
            
            // Call registered callbacks
            for (const auto& callback : callbacks_) {
                try {
                    callback(info);
                } catch (...) {
                    // Ignore callback exceptions during crash handling
                }
            }
            
            // Restore default handler and re-raise
            std::signal(signal, SIG_DFL);
            std::raise(signal);
        }
        
#ifdef _WIN32
        void handleException(EXCEPTION_POINTERS* exceptionInfo) {
            CrashInfo info;
            info.signal = static_cast<int>(exceptionInfo->ExceptionRecord->ExceptionCode);
            info.signalName = getExceptionName(exceptionInfo->ExceptionRecord->ExceptionCode);
            info.timestamp = getCurrentTimestamp();
            info.executable = getExecutablePath();
            info.workingDirectory = getCurrentWorkingDirectory();
            info.stackTrace = getStackTraceWindows(exceptionInfo);
            info.systemInfo = getSystemInfo();
            
            writeCrashLog(info);
            
            for (const auto& callback : callbacks_) {
                try {
                    callback(info);
                } catch (...) {
                    // Ignore callback exceptions
                }
            }
        }
#endif
        
        std::string getSignalName(int signal) const {
            switch (signal) {
                case SIGABRT: return "SIGABRT (Abort)";
                case SIGFPE: return "SIGFPE (Floating Point Exception)";
                case SIGILL: return "SIGILL (Illegal Instruction)";
                case SIGINT: return "SIGINT (Interrupt)";
                case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
                case SIGTERM: return "SIGTERM (Terminate)";
#ifndef _WIN32
                case SIGBUS: return "SIGBUS (Bus Error)";
                case SIGKILL: return "SIGKILL (Kill)";
                case SIGPIPE: return "SIGPIPE (Broken Pipe)";
                case SIGQUIT: return "SIGQUIT (Quit)";
#endif
                default: return "Unknown Signal (" + std::to_string(signal) + ")";
            }
        }
        
#ifdef _WIN32
        std::string getExceptionName(DWORD code) const {
            switch (code) {
                case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
                case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
                case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
                case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
                case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
                case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
                case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
                case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
                case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
                case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
                case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
                case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
                case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
                case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
                case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
                case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
                case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
                case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
                case EXCEPTION_SINGLE_STEP: return "SINGLE_STEP";
                case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
                default: return "Unknown Exception (0x" + std::to_string(code) + ")";
            }
        }
#endif
        
        std::string getCurrentTimestamp() const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
            return std::string(buffer) + "." + std::to_string(ms.count());
        }
        
        std::string getExecutablePath() const {
#ifdef _WIN32
            char path[MAX_PATH];
            GetModuleFileNameA(NULL, path, MAX_PATH);
            return std::string(path);
#else
            char path[1024];
            ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
            if (len != -1) {
                path[len] = '\0';
                return std::string(path);
            }
            return "unknown";
#endif
        }
        
        std::string getCurrentWorkingDirectory() const {
#ifdef _WIN32
            char path[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, path);
            return std::string(path);
#else
            char* cwd = getcwd(nullptr, 0);
            if (cwd) {
                std::string result(cwd);
                free(cwd);
                return result;
            }
            return "unknown";
#endif
        }
        
        std::vector<std::string> getStackTrace() const {
            std::vector<std::string> trace;
            
#ifdef _WIN32
            // Windows stack trace would require DbgHelp
            trace.push_back("Stack trace not available on Windows without DbgHelp");
#else
            void* array[256];
            size_t size = backtrace(array, 256);
            char** strings = backtrace_symbols(array, size);
            
            if (strings) {
                for (size_t i = 0; i < size; ++i) {
                    trace.push_back(std::string(strings[i]));
                }
                free(strings);
            }
#endif
            
            return trace;
        }
        
#ifdef _WIN32
        std::vector<std::string> getStackTraceWindows(EXCEPTION_POINTERS* exceptionInfo) const {
            std::vector<std::string> trace;
            // This would require proper DbgHelp implementation
            trace.push_back("Windows stack trace requires DbgHelp library");
            return trace;
        }
#endif
        
        std::string getSystemInfo() const {
            std::string info;
            
#ifdef _WIN32
            OSVERSIONINFO osvi;
            ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            GetVersionEx(&osvi);
            
            info += "Windows " + std::to_string(osvi.dwMajorVersion) + "." + 
                   std::to_string(osvi.dwMinorVersion) + " Build " + 
                   std::to_string(osvi.dwBuildNumber);
            
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            info += " (" + std::to_string(sysInfo.dwNumberOfProcessors) + " cores)";
#else
            struct utsname unameData;
            if (uname(&unameData) == 0) {
                info += std::string(unameData.sysname) + " " + 
                       std::string(unameData.release) + " " + 
                       std::string(unameData.machine);
            }
#endif
            
            return info;
        }
        
        void writeCrashLog(const CrashInfo& info) const {
            std::ofstream logFile(crashLogPath_, std::ios::app);
            if (!logFile) return;
            
            logFile << "=== CRASH REPORT ===" << std::endl;
            logFile << "Timestamp: " << info.timestamp << std::endl;
            logFile << "Signal: " << info.signalName << std::endl;
            logFile << "Executable: " << info.executable << std::endl;
            logFile << "Working Directory: " << info.workingDirectory << std::endl;
            logFile << "System: " << info.systemInfo << std::endl;
            
            if (!info.additionalInfo.empty()) {
                logFile << "Additional Info: " << info.additionalInfo << std::endl;
            }
            
            logFile << std::endl << "Stack Trace:" << std::endl;
            for (const auto& frame : info.stackTrace) {
                logFile << "  " << frame << std::endl;
            }
            
            logFile << std::endl << "===================" << std::endl << std::endl;
        }
        
    public:
        static CrashHandler& getInstance() {
            if (!instance_) {
                instance_ = new CrashHandler();
            }
            return *instance_;
        }
        
        void initialize(const std::string& crashLogPath = "logs/crash.log") {
            if (initialized_) return;
            
            crashLogPath_ = crashLogPath;
            
            // Create logs directory if it doesn't exist
            size_t pos = crashLogPath.find_last_of("/\\");
            if (pos != std::string::npos) {
                std::string dir = crashLogPath.substr(0, pos);
                // In a real implementation, would use std::filesystem::create_directories
                // For now, assume directory exists
            }
            
            // Install signal handlers
            std::signal(SIGABRT, signalHandler);
            std::signal(SIGFPE, signalHandler);
            std::signal(SIGILL, signalHandler);
            std::signal(SIGINT, signalHandler);
            std::signal(SIGSEGV, signalHandler);
            std::signal(SIGTERM, signalHandler);
            
#ifndef _WIN32
            std::signal(SIGBUS, signalHandler);
            std::signal(SIGQUIT, signalHandler);
#else
            SetUnhandledExceptionFilter(exceptionHandler);
#endif
            
            initialized_ = true;
        }
        
        void addCallback(std::function<void(const CrashInfo&)> callback) {
            callbacks_.push_back(std::move(callback));
        }
        
        void setAdditionalInfo(const std::string& info) {
            // This could be called to add context-specific information
            // For now, just store it for the next crash
        }
    };
    
} // namespace voxelvk