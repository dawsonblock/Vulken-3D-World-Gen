# CMake generated Testfile for 
# Source directory: /Users/dawsonblock/Vulken-3D-World-Gen
# Build directory: /Users/dawsonblock/Vulken-3D-World-Gen/build_debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_shaders_compile]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/test_shaders_compile")
set_tests_properties([=[test_shaders_compile]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;751;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
add_test([=[test_smoke_headless]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/smoke_headless")
set_tests_properties([=[test_smoke_headless]=] PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;754;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
add_test([=[test_weather_system]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/test_weather_system")
set_tests_properties([=[test_weather_system]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;769;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
add_test([=[test_rl_backend]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/test_rl_backend")
set_tests_properties([=[test_rl_backend]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;782;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
add_test([=[test_performance_monitoring]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/test_performance_monitoring")
set_tests_properties([=[test_performance_monitoring]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;796;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
add_test([=[test_integration]=] "/Users/dawsonblock/Vulken-3D-World-Gen/build_debug/test_integration")
set_tests_properties([=[test_integration]=] PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;817;add_test;/Users/dawsonblock/Vulken-3D-World-Gen/CMakeLists.txt;0;")
subdirs("src/engine")
subdirs("tests")
subdirs("tools")
