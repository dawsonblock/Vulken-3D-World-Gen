#include "structured_logger.hpp"
#include <filesystem>
#include <cstdlib>

namespace voxelvk {
    std::unique_ptr<StructuredLogger> StructuredLogger::instance_;
}