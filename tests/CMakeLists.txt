cmake_minimum_required(VERSION 3.20)

enable_testing()

# Add smoke engine test as specified in the plan
add_executable(test_smoke_engine test_smoke_engine.cpp)
target_link_libraries(test_smoke_engine PRIVATE vulken_core)
add_test(NAME smoke_engine COMMAND test_smoke_engine)

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
  message(WARNING "GTest not found - tests will be disabled")
  return()
endif()

# Include GoogleTest module for gtest_discover_tests
include(GoogleTest)

find_package(yaml-cpp CONFIG QUIET)
find_package(glm CONFIG QUIET)

# Individual test executables for better isolation
add_executable(test_smoke_init test_smoke_init.cpp)
target_link_libraries(test_smoke_init PRIVATE GTest::gtest_main Vulkan::Vulkan)
gtest_discover_tests(test_smoke_init)

# Enhanced headless swapchain test
add_executable(test_headless_swapchain test_headless_swapchain.cpp)
target_link_libraries(test_headless_swapchain PRIVATE GTest::gtest_main Vulkan::Vulkan)
gtest_discover_tests(test_headless_swapchain)

add_executable(test_math_util test_math_util.cpp)
target_compile_features(test_math_util PRIVATE cxx_std_20)
target_include_directories(test_math_util PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_math_util PRIVATE GTest::gtest_main)
if (glm_FOUND)
  target_compile_definitions(test_math_util PRIVATE HAS_GLM=1)
  target_link_libraries(test_math_util PRIVATE glm::glm)
endif()
gtest_discover_tests(test_math_util)

add_executable(voxelvk_tests
  # GoogleTest-based unit tests only (no standalone mains)
  test_smoke.cpp
  test_math_geometry.cpp
  test_yaml_config.cpp
  test_yaml_schema.cpp
  test_frustum_cull.cpp
  test_ray_aabb.cpp
  test_cpp_physics.cpp
  test_dda.cpp
)

target_compile_features(voxelvk_tests PRIVATE cxx_std_20)
target_compile_definitions(voxelvk_tests PRIVATE SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
target_include_directories(voxelvk_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link required engine sources for physics and DDA tests
target_sources(voxelvk_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src/physics/cpp/aabb.cpp
  ${CMAKE_SOURCE_DIR}/src/physics/cpp/capsule.cpp
  ${CMAKE_SOURCE_DIR}/src/physics/cpp/collision_utils.cpp
  ${CMAKE_SOURCE_DIR}/src/physics/cpp/voxel_solid.cpp
  ${CMAKE_SOURCE_DIR}/src/env/raycast_dda.cc
  ${CMAKE_SOURCE_DIR}/src/env/world_manager.cc
  ${CMAKE_SOURCE_DIR}/src/env/blocks.cc
  ${CMAKE_SOURCE_DIR}/src/core/logger.cpp
  ${CMAKE_SOURCE_DIR}/src/core/thread_pool.cpp
)

target_link_libraries(voxelvk_tests PRIVATE GTest::gtest_main)
if (TARGET yaml-cpp::yaml-cpp)
  target_compile_definitions(voxelvk_tests PRIVATE HAS_YAML_CPP=1)
  target_link_libraries(voxelvk_tests PRIVATE yaml-cpp::yaml-cpp)
elseif(yaml-cpp_FOUND)
  # Legacy target alias
  target_compile_definitions(voxelvk_tests PRIVATE HAS_YAML_CPP=1)
  target_link_libraries(voxelvk_tests PRIVATE yaml-cpp)
endif()
if (glm_FOUND)
  target_compile_definitions(voxelvk_tests PRIVATE HAS_GLM=1)
  target_link_libraries(voxelvk_tests PRIVATE glm::glm)
endif()

include(GoogleTest)
gtest_discover_tests(voxelvk_tests)

# Add performance tests (if dependencies available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/perf")
  add_subdirectory(perf)
endif()

# Add image regression tests (if dependencies available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/render_diff")
  add_subdirectory(render_diff)
endif()
