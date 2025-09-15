cmake_minimum_required(VERSION 3.20)

enable_testing()

add_executable(test_smoke_engine test_smoke_engine.cpp)
add_test(NAME smoke_engine COMMAND test_smoke_engine)
