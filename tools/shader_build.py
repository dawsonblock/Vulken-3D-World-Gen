#!/usr/bin/env python3
"""
VoxelVK Shader Build Pipeline
Discovers shaders, compiles to SPIR-V, runs spirv-opt optimization, and generates manifest.
"""

import os
import sys
import json
import subprocess
import argparse
from pathlib import Path
from typing import List, Dict, Any, Optional
import hashlib

class ShaderBuilder:
    def __init__(self, source_dirs: List[str], output_dir: str, tools_dir: Optional[str] = None):
        self.source_dirs = [Path(d) for d in source_dirs]
        self.output_dir = Path(output_dir)
        self.tools_dir = Path(tools_dir) if tools_dir else None
        self.manifest = {
            "version": "1.0",
            "generated_by": "VoxelVK Shader Build Pipeline",
            "shaders": {},
            "statistics": {
                "total_shaders": 0,
                "compiled_successfully": 0,
                "compilation_errors": 0,
                "optimization_errors": 0
            }
        }
        
    def find_shader_tools(self) -> Dict[str, str]:
        """Find shader compilation tools (glslc, spirv-opt)"""
        tools = {}
        
        # Common tool locations
        tool_paths = [
            os.environ.get('VULKAN_SDK', ''),
            '/usr/bin',
            '/usr/local/bin',
            str(self.tools_dir) if self.tools_dir else '',
        ]
        
        # Add vcpkg paths
        vcpkg_paths = [
            'vcpkg/installed/x64-linux/tools/shaderc',
            'vcpkg/installed/x64-windows/tools/shaderc',
            'build/vcpkg_installed/x64-linux/tools/shaderc',
            'build/vcpkg_installed/x64-windows/tools/shaderc',
        ]
        tool_paths.extend(vcpkg_paths)
        
        for tool_name in ['glslc', 'spirv-opt']:
            for path in tool_paths:
                if path:
                    tool_path = Path(path) / tool_name
                    if tool_path.exists() and tool_path.is_file():
                        tools[tool_name] = str(tool_path)
                        break
            
            if tool_name not in tools:
                # Try system PATH
                try:
                    result = subprocess.run(['which', tool_name], capture_output=True, text=True)
                    if result.returncode == 0:
                        tools[tool_name] = result.stdout.strip()
                except:
                    pass
        
        return tools
    
    def discover_shaders(self) -> List[Path]:
        """Discover all shader files in source directories"""
        shader_extensions = {'.vert', '.frag', '.comp', '.glsl'}
        shaders = []
        
        for source_dir in self.source_dirs:
            if not source_dir.exists():
                print(f"Warning: Source directory {source_dir} does not exist")
                continue
                
            for shader_file in source_dir.rglob('*'):
                if shader_file.is_file() and shader_file.suffix in shader_extensions:
                    # Skip .glsl files that are likely includes
                    if shader_file.suffix == '.glsl' and not any(
                        part in shader_file.name for part in ['.vert', '.frag', '.comp']
                    ):
                        continue
                    shaders.append(shader_file)
        
        return sorted(shaders)
    
    def get_shader_stage(self, shader_path: Path) -> str:
        """Determine shader stage from filename"""
        name = shader_path.name.lower()
        
        if '.vert' in name or name.endswith('.vert'):
            return 'vertex'
        elif '.frag' in name or name.endswith('.frag'):
            return 'fragment'
        elif '.comp' in name or name.endswith('.comp'):
            return 'compute'
        elif '.geom' in name or name.endswith('.geom'):
            return 'geometry'
        elif '.tesc' in name or name.endswith('.tesc'):
            return 'tesscontrol'
        elif '.tese' in name or name.endswith('.tese'):
            return 'tesseval'
        else:
            return 'unknown'
    
    def calculate_hash(self, file_path: Path) -> str:
        """Calculate SHA256 hash of file content"""
        with open(file_path, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()
    
    def compile_shader(self, shader_path: Path, output_path: Path, tools: Dict[str, str]) -> bool:
        """Compile a single shader to SPIR-V"""
        try:
            # Create output directory
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Determine shader stage
            stage = self.get_shader_stage(shader_path)
            if stage == 'unknown':
                print(f"Warning: Unknown shader stage for {shader_path}")
                return False
            
            # Compile with glslc
            glslc_cmd = [
                tools['glslc'],
                f'--target-env=vulkan1.3',
                f'-fshader-stage={stage}',
                '-O',  # Optimize
                str(shader_path),
                '-o', str(output_path)
            ]
            
            result = subprocess.run(glslc_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"Error compiling {shader_path}:")
                print(result.stderr)
                return False
            
            # Run spirv-opt optimization
            if 'spirv-opt' in tools:
                opt_path = output_path.with_suffix('.opt.spv')
                opt_cmd = [
                    tools['spirv-opt'],
                    '-O',  # Optimize
                    str(output_path),
                    '-o', str(opt_path)
                ]
                
                opt_result = subprocess.run(opt_cmd, capture_output=True, text=True)
                if opt_result.returncode == 0:
                    # Replace original with optimized version
                    opt_path.replace(output_path)
                else:
                    print(f"Warning: spirv-opt failed for {shader_path}: {opt_result.stderr}")
            
            return True
            
        except Exception as e:
            print(f"Exception compiling {shader_path}: {e}")
            return False
    
    def build_shaders(self) -> bool:
        """Build all discovered shaders"""
        print("VoxelVK Shader Build Pipeline")
        print("=" * 40)
        
        # Find tools
        tools = self.find_shader_tools()
        if 'glslc' not in tools:
            print("Error: glslc not found. Install Vulkan SDK or shaderc tools.")
            return False
        
        print(f"Using glslc: {tools['glslc']}")
        if 'spirv-opt' in tools:
            print(f"Using spirv-opt: {tools['spirv-opt']}")
        else:
            print("Warning: spirv-opt not found, skipping optimization")
        
        # Discover shaders
        shaders = self.discover_shaders()
        print(f"Found {len(shaders)} shader files")
        
        if not shaders:
            print("No shaders found to compile")
            return True
        
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Compile each shader
        success_count = 0
        error_count = 0
        
        for shader_path in shaders:
            # Calculate relative path for output
            rel_path = None
            for source_dir in self.source_dirs:
                try:
                    rel_path = shader_path.relative_to(source_dir)
                    break
                except ValueError:
                    continue
            
            if not rel_path:
                print(f"Warning: Could not determine relative path for {shader_path}")
                continue
            
            output_path = self.output_dir / rel_path.with_suffix('.spv')
            
            print(f"Compiling: {rel_path} -> {output_path.relative_to(self.output_dir)}")
            
            if self.compile_shader(shader_path, output_path, tools):
                success_count += 1
                
                # Add to manifest
                shader_info = {
                    "source_path": str(shader_path),
                    "output_path": str(output_path.relative_to(self.output_dir)),
                    "stage": self.get_shader_stage(shader_path),
                    "hash": self.calculate_hash(shader_path),
                    "compiled": True
                }
                
                manifest_key = str(rel_path).replace('/', '_').replace('\\', '_')
                self.manifest["shaders"][manifest_key] = shader_info
            else:
                error_count += 1
                print(f"Failed to compile: {shader_path}")
        
        # Update statistics
        self.manifest["statistics"]["total_shaders"] = len(shaders)
        self.manifest["statistics"]["compiled_successfully"] = success_count
        self.manifest["statistics"]["compilation_errors"] = error_count
        
        # Write manifest
        manifest_path = self.output_dir / "shader_manifest.json"
        with open(manifest_path, 'w') as f:
            json.dump(self.manifest, f, indent=2)
        
        print(f"\nCompilation complete:")
        print(f"  Total shaders: {len(shaders)}")
        print(f"  Compiled successfully: {success_count}")
        print(f"  Compilation errors: {error_count}")
        print(f"  Manifest written to: {manifest_path}")
        
        return error_count == 0

def main():
    parser = argparse.ArgumentParser(description='VoxelVK Shader Build Pipeline')
    parser.add_argument('--source-dirs', nargs='+', default=['shaders', 'shaders_vk'],
                        help='Source directories to search for shaders')
    parser.add_argument('--output-dir', default='resources/spv',
                        help='Output directory for compiled SPIR-V files')
    parser.add_argument('--tools-dir', help='Directory containing shader tools')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose output')
    
    args = parser.parse_args()
    
    builder = ShaderBuilder(args.source_dirs, args.output_dir, args.tools_dir)
    success = builder.build_shaders()
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()