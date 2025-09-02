cmake_minimum_required(VERSION 3.20)

enable_testing()
find_package(GTest CONFIG REQUIRED)
find_package(yaml-cpp CONFIG QUIET)
find_package(glm CONFIG QUIET)

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
if (yaml-cpp_FOUND)
  target_compile_definitions(voxelvk_tests PRIVATE HAS_YAML_CPP=1)
  target_link_libraries(voxelvk_tests PRIVATE yaml-cpp)
endif()
if (glm_FOUND)
  target_compile_definitions(voxelvk_tests PRIVATE HAS_GLM=1)
  target_link_libraries(voxelvk_tests PRIVATE glm::glm)
endif()

include(GoogleTest)
gtest_discover_tests(voxelvk_tests)
