cmake_minimum_required(VERSION 3.21)

project(vulken_engine LANGUAGES CXX)

file(GLOB_RECURSE VULKEN_ENGINE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

add_library(vulken_engine STATIC ${VULKEN_ENGINE_SOURCES})

target_include_directories(vulken_engine
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(vulken_engine PUBLIC cxx_std_20)

# Optional: make this configurable later
target_compile_definitions(vulken_engine PUBLIC VULKEN_ENGINE_ENABLE_LOGGING=1)

# Dependencies (link real Vulkan, vk_mem_alloc, stb, etc. later as needed)
# target_link_libraries(vulken_engine PUBLIC Vulkan::Vulkan vk_mem_alloc stb)
