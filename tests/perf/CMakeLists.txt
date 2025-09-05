# Performance Budget Tests
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(JSONCPP QUIET jsoncpp)
endif()

if(NOT JSONCPP_FOUND)
  message(WARNING "jsoncpp not found - performance tests will be disabled")
  return()
endif()

# Performance harness executable
add_executable(performance_harness performance_harness.cpp)

target_include_directories(performance_harness PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(performance_harness PRIVATE
    ${JSONCPP_LIBRARIES}
    vulkan
    glfw
)

target_compile_definitions(performance_harness PRIVATE
    VOXELVK_PERFORMANCE_TEST=1
)

# Add performance test to CTest
add_test(NAME performance_budget_headless
    COMMAND performance_harness --headless --config ${CMAKE_CURRENT_SOURCE_DIR}/frame_budget.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Set test properties
set_tests_properties(performance_budget_headless PROPERTIES
    TIMEOUT 300
    LABELS "performance"
)

# Install performance budget configuration
install(FILES frame_budget.json
    DESTINATION share/voxelvk/performance
)