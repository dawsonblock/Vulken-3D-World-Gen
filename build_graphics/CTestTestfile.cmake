# CMake generated Testfile for 
# Source directory: /Users/dawsonblock/Vulken-3D-World-Gen-4
# Build directory: /Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_shaders_compile]=] "/Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics/test_shaders_compile")
set_tests_properties([=[test_shaders_compile]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;820;add_test;/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;0;")
add_test([=[test_weather_system]=] "/Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics/test_weather_system")
set_tests_properties([=[test_weather_system]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;835;add_test;/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;0;")
add_test([=[test_rl_backend]=] "/Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics/test_rl_backend")
set_tests_properties([=[test_rl_backend]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;848;add_test;/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;0;")
add_test([=[test_performance_monitoring]=] "/Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics/test_performance_monitoring")
set_tests_properties([=[test_performance_monitoring]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;865;add_test;/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;0;")
add_test([=[test_integration]=] "/Users/dawsonblock/Vulken-3D-World-Gen-4/build_graphics/test_integration")
set_tests_properties([=[test_integration]=] PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;889;add_test;/Users/dawsonblock/Vulken-3D-World-Gen-4/CMakeLists.txt;0;")
subdirs("src/engine")
subdirs("tests")
subdirs("apps")
subdirs("tools")
