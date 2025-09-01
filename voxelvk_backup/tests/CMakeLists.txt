cmake_minimum_required(VERSION 3.20)

enable_testing()
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
  message(WARNING "GTest not found via CONFIG; skipping C++ unit tests build")
  return()
endif()
find_package(yaml-cpp CONFIG QUIET)
find_package(glm CONFIG QUIET)

add_executable(voxelvk_tests
    test_smoke.cpp
    test_math_geometry.cpp
    test_yaml_config.cpp
    test_yaml_schema.cpp
    test_frustum_cull.cpp
    test_ray_aabb.cpp
)

target_compile_features(voxelvk_tests PRIVATE cxx_std_20)
target_compile_definitions(voxelvk_tests PRIVATE SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

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
