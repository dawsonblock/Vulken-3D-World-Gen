# Image Regression Testing
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(STB QUIET stb)
endif()

if(NOT STB_FOUND)
  message(WARNING "stb not found - image regression tests will be disabled")
  return()
endif()

# Minimal renderer for generating test images
add_executable(minimal_renderer minimal_renderer.cpp)

target_include_directories(minimal_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minimal_renderer PRIVATE
    vulkan
    glfw
    ${STB_LIBRARIES}
)

target_compile_definitions(minimal_renderer PRIVATE
    STB_IMAGE_WRITE_IMPLEMENTATION
)

# Image regression test script
add_test(NAME image_regression_test
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/image_regression.py
        --golden-dir ${CMAKE_CURRENT_SOURCE_DIR}/golden
        --test-dir ${CMAKE_CURRENT_SOURCE_DIR}/test
        --tolerance 0.98
        --output-report ${CMAKE_BINARY_DIR}/render_diff_report.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Set test properties
set_tests_properties(image_regression_test PROPERTIES
    TIMEOUT 60
    LABELS "regression;rendering"
)

# Install test images and scripts
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION share/voxelvk/render_diff
    FILES_MATCHING PATTERN "*.py"
)

# Create directories for test images
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/golden)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/diffs)