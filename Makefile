# Simple Makefile for macOS local builds (no vcpkg)
# Builds: world_generator, world_visualizer, rl_nav_demo

CXX := clang++
CXXFLAGS := -std=c++20 -O2 -Isrc
LDFLAGS :=

.PHONY: all demos world_generator world_visualizer rl_nav_demo clean run_demo server server8081

all: demos

demos: world_generator world_visualizer rl_nav_demo

world_generator: world_generator.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

world_visualizer: world_visualizer.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

rl_nav_demo: build_minimal/rl_nav_demo

build_minimal/rl_nav_demo: apps/rl_nav_demo.cpp src/rl/rl_backend_minimal.cpp src/rl/rl_backend_dummy.cpp src/core/logger.cpp
	@mkdir -p build_minimal
	$(CXX) $(CXXFLAGS) $^ -o $@

run_demo: demos
	./video_demo.sh

# Start the local demo server (defaults to port 8081)
PORT ?= 8081
server:
	python3 demo_server.py --port $(PORT)

server8081:
	PORT=8081 $(MAKE) server

clean:
	rm -f world_generator world_visualizer
	rm -rf build_minimal
	rm -f world_map.txt world_viewer.html navigation_policy.vxml
