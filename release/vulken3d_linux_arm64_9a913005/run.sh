#!/bin/bash
# Vulken-3D Quick Start Script
echo "Starting Vulken-3D Engine..."

# Set environment variables
export VULKAN_SDK="${PWD}"
export PATH="${PWD}/bin:${PATH}"

# Make binaries executable
chmod +x bin/*

# Run the headless demo
echo "Running headless graphics demo..."
./bin/smoke_graphics_headless --config config/engine.yaml --headless

# Optional: Run operator console
# ./bin/operator_console

echo "Done."
