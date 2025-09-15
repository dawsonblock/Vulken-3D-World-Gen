cmake_minimum_required(VERSION 3.20)

enable_testing()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_smoke_engine.cpp")
  add_executable(test_smoke_engine test_smoke_engine.cpp)
  add_test(NAME smoke_engine COMMAND test_smoke_engine)
else()
  message(WARNING "ENABLE_TESTS is ON but no test sources were found in tests/")
endif()
