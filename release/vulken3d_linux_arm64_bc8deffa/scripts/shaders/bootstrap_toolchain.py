#!/usr/bin/env python3
"""
Vulkan Shader Toolchain Bootstrap Script
========================================

This script sets up the shader compilation toolchain by:
1. Checking for required tools (glslc, spirv-cross)
2. Setting up CMake integration for shader compilation  
3. Creating initial test shaders and validation

Phase 3 of the Production Upgrade Plan.
"""

import os
import sys
import shutil
import subprocess
from pathlib import Path


class ShaderToolchainBootstrapper:
    def __init__(self):
        self.required_tools = {
            "glslc": "Shader compiler from Vulkan SDK",
            "spirv-cross": "SPIR-V reflection tool"
        }
        
    def check_tool_availability(self) -> bool:
        """Check if required shader tools are available."""
        print("🔧 Checking shader toolchain availability...")
        
        all_available = True
        
        for tool, description in self.required_tools.items():
            tool_path = shutil.which(tool)
            if tool_path:
                print(f"  ✅ {tool}: {tool_path}")
                # Get version info
                try:
                    result = subprocess.run([tool, "--version"], 
                                          capture_output=True, text=True)
                    if result.returncode == 0:
                        version = result.stdout.strip().split('\n')[0]
                        print(f"     Version: {version}")
                except Exception:
                    pass
            else:
                print(f"  ❌ {tool}: Not found ({description})")
                all_available = False
        
        return all_available
    
    def suggest_installation(self) -> None:
        """Provide installation suggestions for missing tools."""
        print("\n💡 Installation suggestions:")
        print("  1. Install Vulkan SDK from https://vulkan.lunarg.com/")
        print("  2. Ensure VULKAN_SDK environment variable is set")
        print("  3. Add $VULKAN_SDK/bin to your PATH")
        print("  4. On Ubuntu: sudo apt install vulkan-sdk")
        print("  5. On macOS: brew install vulkan-sdk")
    
    def test_shader_compilation(self) -> bool:
        """Test basic shader compilation functionality."""
        print("\n🧪 Testing shader compilation...")
        
        # Create a simple test vertex shader
        test_shader_content = """#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}"""
        
        # Write test shader to temporary file
        test_file = Path("test_shader.vert")
        output_file = Path("test_shader.spv")
        
        try:
            with open(test_file, 'w') as f:
                f.write(test_shader_content)
            
            # Compile with glslc
            result = subprocess.run([
                "glslc", 
                "--target-env=vulkan1.2",
                "-o", str(output_file),
                str(test_file)
            ], capture_output=True, text=True)
            
            if result.returncode == 0 and output_file.exists():
                print("  ✅ Basic shader compilation successful")
                
                # Test spirv-cross reflection
                if shutil.which("spirv-cross"):
                    reflect_result = subprocess.run([
                        "spirv-cross", str(output_file), "--reflect", "--json"
                    ], capture_output=True, text=True)
                    
                    if reflect_result.returncode == 0:
                        print("  ✅ SPIR-V reflection successful")
                        return True
                    else:
                        print(f"  ⚠️  SPIR-V reflection failed: {reflect_result.stderr}")
                        return False
                else:
                    print("  ⚠️  spirv-cross not available, skipping reflection test")
                    return True
                    
            else:
                print(f"  ❌ Shader compilation failed: {result.stderr}")
                return False
                
        except Exception as e:
            print(f"  ❌ Test compilation error: {e}")
            return False
        finally:
            # Clean up test files
            for temp_file in [test_file, output_file]:
                if temp_file.exists():
                    temp_file.unlink()
    
    def setup_cmake_integration(self) -> bool:
        """Set up CMake integration for shader compilation."""
        print("\n⚙️  Setting up CMake shader integration...")
        
        # Check if CMakeLists.txt exists
        cmake_file = Path("CMakeLists.txt")
        if not cmake_file.exists():
            print("  ⚠️  CMakeLists.txt not found, skipping CMake integration")
            return False
        
        # Read current CMakeLists.txt
        try:
            with open(cmake_file, 'r') as f:
                cmake_content = f.read()
            
            # Check if shader module is already included
            if "cmake/shaders.cmake" in cmake_content:
                print("  ✅ Shader CMake module already included")
                return True
            
            # Add shader module inclusion
            shader_include = "\n# Shader compilation support\ninclude(cmake/shaders.cmake)\nsetup_shader_compilation(VALIDATE_LAYOUT)\n"
            
            # Find a good place to insert (after project declaration)
            lines = cmake_content.split('\n')
            insert_index = -1
            
            for i, line in enumerate(lines):
                if line.strip().startswith("project("):
                    insert_index = i + 1
                    break
            
            if insert_index >= 0:
                lines.insert(insert_index, shader_include)
                
                # Write back to file
                with open(cmake_file, 'w') as f:
                    f.write('\n'.join(lines))
                
                print("  ✅ Added shader compilation to CMakeLists.txt")
                return True
            else:
                print("  ⚠️  Could not find project() declaration in CMakeLists.txt")
                print("     Please manually add: include(cmake/shaders.cmake)")
                return False
                
        except Exception as e:
            print(f"  ❌ Failed to modify CMakeLists.txt: {e}")
            return False
    
    def create_validation_script(self) -> None:
        """Create a script for continuous shader validation."""
        script_content = """#!/bin/bash
# Shader Validation Script
set -e

echo "🔧 Building shaders..."
cmake --build build --target compile_all_shaders

echo "🔍 Validating shader layouts..."
python3 scripts/shaders/validate_layout.py \\
    --shader-dirs build/shaders_cache \\
    --cpp-dirs src include \\
    --report reports/shaders/layout_check.json

echo "✅ Shader validation complete"
"""
        
        script_path = Path("scripts/shaders/validate_all.sh")
        script_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(script_path, 'w') as f:
            f.write(script_content)
        
        # Make executable
        os.chmod(script_path, 0o755)
        print(f"  ✅ Created validation script: {script_path}")
    
    def bootstrap(self) -> bool:
        """Run complete toolchain bootstrap process."""
        print("🚀 Bootstrapping Vulkan shader toolchain")
        
        # Check tool availability
        if not self.check_tool_availability():
            self.suggest_installation()
            return False
        
        # Test compilation
        if not self.test_shader_compilation():
            print("❌ Shader compilation test failed")
            return False
        
        # Set up CMake integration
        self.setup_cmake_integration()
        
        # Create validation script
        self.create_validation_script()
        
        print("\n✅ Shader toolchain bootstrap complete!")
        print("📋 Next steps:")
        print("   1. Run: cmake --preset default")
        print("   2. Run: cmake --build build --target compile_all_shaders")
        print("   3. Run: python3 scripts/shaders/validate_layout.py")
        
        return True


def main():
    bootstrapper = ShaderToolchainBootstrapper()
    success = bootstrapper.bootstrap()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()