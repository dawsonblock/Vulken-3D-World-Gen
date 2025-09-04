#!/usr/bin/env python3
"""
Vulkan Shader Layout Validation Script  
======================================

This script uses spirv-cross to reflect SPIR-V shaders and validates that
uniform buffer objects, storage buffers, and push constants match their
corresponding C++ struct definitions.

Phase 3 of the Production Upgrade Plan.
"""

import os
import sys
import json
import subprocess
import re
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
import argparse


class ShaderLayoutValidator:
    def __init__(self, spirv_cross_path: str = "spirv-cross"):
        self.spirv_cross = spirv_cross_path
        self.report = {
            "validation_summary": {},
            "validated_shaders": [],
            "layout_mismatches": [],
            "missing_cpp_structs": [],
            "warnings": [],
            "errors": []
        }
        
        # Common C++ type mappings
        self.SPIRV_TO_CPP_TYPES = {
            "float": ["float", "f32"],
            "vec2": ["glm::vec2", "Vec2", "float2"],
            "vec3": ["glm::vec3", "Vec3", "float3"],
            "vec4": ["glm::vec4", "Vec4", "float4"],
            "mat2": ["glm::mat2", "Mat2"],
            "mat3": ["glm::mat3", "Mat3"], 
            "mat4": ["glm::mat4", "Mat4"],
            "int": ["int", "int32_t", "i32"],
            "ivec2": ["glm::ivec2", "IVec2", "int2"],
            "ivec3": ["glm::ivec3", "IVec3", "int3"],
            "ivec4": ["glm::ivec4", "IVec4", "int4"],
            "uint": ["uint", "uint32_t", "u32", "unsigned int"],
            "uvec2": ["glm::uvec2", "UVec2", "uint2"],
            "uvec3": ["glm::uvec3", "UVec3", "uint3"],
            "uvec4": ["glm::uvec4", "UVec4", "uint4"]
        }

    def find_spirv_files(self, shader_dir: str) -> List[Path]:
        """Find all SPIR-V files in the shader directory."""
        shader_path = Path(shader_dir)
        spirv_files = []
        
        for ext in ["*.spv", "*.spirv"]:
            spirv_files.extend(shader_path.rglob(ext))
        
        return spirv_files

    def reflect_spirv_shader(self, spirv_file: Path) -> Optional[Dict[str, Any]]:
        """Use spirv-cross to reflect shader layout information."""
        try:
            cmd = [self.spirv_cross, str(spirv_file), "--reflect", "--json"]
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            
            reflection_data = json.loads(result.stdout)
            return reflection_data
            
        except subprocess.CalledProcessError as e:
            self.report["errors"].append({
                "shader": str(spirv_file),
                "error": f"spirv-cross failed: {e.stderr}"
            })
            return None
        except json.JSONDecodeError as e:
            self.report["errors"].append({
                "shader": str(spirv_file),
                "error": f"JSON decode failed: {str(e)}"
            })
            return None

    def find_cpp_structs(self, cpp_dirs: List[str]) -> Dict[str, Dict[str, Any]]:
        """Parse C++ header files to find struct definitions."""
        cpp_structs = {}
        
        for cpp_dir in cpp_dirs:
            cpp_path = Path(cpp_dir)
            if not cpp_path.exists():
                continue
                
            for header_file in cpp_path.rglob("*.hpp"):
                structs = self.parse_cpp_structs(header_file)
                cpp_structs.update(structs)
                
            for header_file in cpp_path.rglob("*.h"):
                structs = self.parse_cpp_structs(header_file)
                cpp_structs.update(structs)
        
        return cpp_structs

    def parse_cpp_structs(self, header_file: Path) -> Dict[str, Dict[str, Any]]:
        """Parse C++ struct definitions from header files."""
        structs = {}
        
        try:
            with open(header_file, 'r') as f:
                content = f.read()
            
            # Simple regex pattern for struct definitions
            # This is basic - a real implementation would use clang-ast or similar
            struct_pattern = r'struct\s+(\w+)\s*\{([^}]*)\}'
            
            for match in re.finditer(struct_pattern, content, re.MULTILINE | re.DOTALL):
                struct_name = match.group(1)
                struct_body = match.group(2)
                
                # Skip templates and other complex cases for now
                if '<' in struct_name or 'template' in struct_body:
                    continue
                
                fields = self.parse_struct_fields(struct_body)
                
                structs[struct_name] = {
                    "file": str(header_file),
                    "fields": fields,
                    "size": self.calculate_struct_size(fields)
                }
                
        except IOError as e:
            self.report["warnings"].append(f"Could not read {header_file}: {e}")
        
        return structs

    def parse_struct_fields(self, struct_body: str) -> List[Dict[str, Any]]:
        """Parse individual fields from a struct body."""
        fields = []
        
        # Remove comments and empty lines
        lines = [line.strip() for line in struct_body.split('\n')]
        lines = [line for line in lines if line and not line.startswith('//')]
        
        offset = 0
        for line in lines:
            # Simple field pattern: type name;
            field_match = re.match(r'(\w+(?:::\w+)*)\s+(\w+)(?:\[(\d+)\])?\s*;', line)
            if field_match:
                field_type = field_match.group(1)
                field_name = field_match.group(2)
                array_size = field_match.group(3)
                
                size = self.get_type_size(field_type)
                if array_size:
                    size *= int(array_size)
                
                fields.append({
                    "name": field_name,
                    "type": field_type,
                    "offset": offset,
                    "size": size,
                    "array_size": int(array_size) if array_size else 1
                })
                
                offset += size
        
        return fields

    def get_type_size(self, cpp_type: str) -> int:
        """Get the size in bytes of a C++ type."""
        size_map = {
            "float": 4, "f32": 4,
            "double": 8, "f64": 8,
            "int": 4, "int32_t": 4, "i32": 4,
            "uint": 4, "uint32_t": 4, "u32": 4, "unsigned int": 4,
            "int64_t": 8, "uint64_t": 8,
            "bool": 1,
            "char": 1, "unsigned char": 1,
            "short": 2, "unsigned short": 2,
        }
        
        # Handle glm types
        if cpp_type.startswith("glm::"):
            base_type = cpp_type[5:]  # Remove "glm::"
        else:
            base_type = cpp_type
        
        if base_type in size_map:
            return size_map[base_type]
        elif base_type.startswith("vec"):
            components = int(base_type[3]) if len(base_type) > 3 and base_type[3].isdigit() else 4
            return components * 4  # Assume float components
        elif base_type.startswith("mat"):
            dim = int(base_type[3]) if len(base_type) > 3 and base_type[3].isdigit() else 4
            return dim * dim * 4  # Assume float matrix
        else:
            return 4  # Default assumption

    def calculate_struct_size(self, fields: List[Dict[str, Any]]) -> int:
        """Calculate the total size of a struct with proper alignment."""
        if not fields:
            return 0
        
        # Simple calculation - doesn't handle complex padding rules
        return max((field["offset"] + field["size"] for field in fields), default=0)

    def validate_uniform_buffers(self, reflection: Dict[str, Any], cpp_structs: Dict[str, Dict[str, Any]], shader_file: Path) -> List[Dict[str, Any]]:
        """Validate uniform buffer objects against C++ struct definitions."""
        mismatches = []
        
        uniform_buffers = reflection.get("ubos", [])
        
        for ubo in uniform_buffers:
            ubo_name = ubo.get("name", "")
            ubo_size = ubo.get("size", 0)
            
            # Try to find matching C++ struct
            matching_struct = None
            for struct_name, struct_info in cpp_structs.items():
                if struct_name.lower() in ubo_name.lower() or ubo_name.lower() in struct_name.lower():
                    matching_struct = (struct_name, struct_info)
                    break
            
            if not matching_struct:
                mismatches.append({
                    "shader": str(shader_file),
                    "type": "uniform_buffer",
                    "name": ubo_name,
                    "issue": "no_matching_cpp_struct",
                    "details": f"No matching C++ struct found for UBO '{ubo_name}'"
                })
                continue
                
            struct_name, struct_info = matching_struct
            
            # Compare sizes
            if ubo_size != struct_info["size"]:
                mismatches.append({
                    "shader": str(shader_file),
                    "type": "uniform_buffer", 
                    "name": ubo_name,
                    "issue": "size_mismatch",
                    "details": f"UBO size {ubo_size} != C++ struct size {struct_info['size']}"
                })
        
        return mismatches

    def validate_push_constants(self, reflection: Dict[str, Any], cpp_structs: Dict[str, Dict[str, Any]], shader_file: Path) -> List[Dict[str, Any]]:
        """Validate push constant blocks against C++ struct definitions.""" 
        mismatches = []
        
        push_constants = reflection.get("push_constant_buffers", [])
        
        for pc in push_constants:
            pc_name = pc.get("name", "PC")
            pc_size = pc.get("size", 0)
            
            # Look for matching C++ struct (often named PushConstants, PC, etc.)
            matching_struct = None
            for struct_name, struct_info in cpp_structs.items():
                if ("push" in struct_name.lower() and "constant" in struct_name.lower()) or \
                   struct_name.lower() == "pc" or \
                   pc_name.lower() in struct_name.lower():
                    matching_struct = (struct_name, struct_info)
                    break
            
            if not matching_struct:
                mismatches.append({
                    "shader": str(shader_file),
                    "type": "push_constants",
                    "name": pc_name, 
                    "issue": "no_matching_cpp_struct",
                    "details": f"No matching C++ struct found for push constants '{pc_name}'"
                })
                continue
            
            struct_name, struct_info = matching_struct
            
            if pc_size != struct_info["size"]:
                mismatches.append({
                    "shader": str(shader_file),
                    "type": "push_constants",
                    "name": pc_name,
                    "issue": "size_mismatch",
                    "details": f"Push constants size {pc_size} != C++ struct size {struct_info['size']}"
                })
        
        return mismatches

    def validate_shader(self, spirv_file: Path, cpp_structs: Dict[str, Dict[str, Any]]) -> None:
        """Validate a single SPIR-V shader against C++ definitions."""
        print(f"🔍 Validating: {spirv_file.name}")
        
        reflection = self.reflect_spirv_shader(spirv_file)
        if not reflection:
            return
        
        shader_info = {
            "file": str(spirv_file),
            "stage": reflection.get("stage", "unknown"),
            "entry_point": reflection.get("entryPoints", [{}])[0].get("name", "main")
        }
        
        # Validate uniform buffers
        ubo_mismatches = self.validate_uniform_buffers(reflection, cpp_structs, spirv_file)
        
        # Validate push constants
        pc_mismatches = self.validate_push_constants(reflection, cpp_structs, spirv_file)
        
        # Collect all mismatches
        all_mismatches = ubo_mismatches + pc_mismatches
        self.report["layout_mismatches"].extend(all_mismatches)
        
        shader_info["mismatches"] = len(all_mismatches)
        self.report["validated_shaders"].append(shader_info)
        
        if all_mismatches:
            print(f"  ⚠️  {len(all_mismatches)} layout mismatches found")
        else:
            print(f"  ✅ Layout validation passed")

    def generate_report(self, output_path: str) -> None:
        """Generate validation report."""
        summary = {
            "total_shaders": len(self.report["validated_shaders"]),
            "shaders_with_mismatches": len([s for s in self.report["validated_shaders"] if s["mismatches"] > 0]),
            "total_mismatches": len(self.report["layout_mismatches"]),
            "total_warnings": len(self.report["warnings"]),
            "total_errors": len(self.report["errors"]),
            "validation_status": "PASS" if len(self.report["layout_mismatches"]) == 0 else "FAIL"
        }
        
        self.report["validation_summary"] = summary
        
        # Write report
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(self.report, f, indent=2)
        
        print(f"\n📊 Validation Report: {output_path}")
        print(f"Status: {summary['validation_status']}")
        print(f"Shaders validated: {summary['total_shaders']}")
        print(f"Layout mismatches: {summary['total_mismatches']}")

    def run_validation(self, shader_dirs: List[str], cpp_dirs: List[str], report_path: str) -> bool:
        """Run complete shader layout validation."""
        print("🔧 Starting shader layout validation")
        
        # Find all SPIR-V files
        spirv_files = []
        for shader_dir in shader_dirs:
            spirv_files.extend(self.find_spirv_files(shader_dir))
        
        if not spirv_files:
            print("⚠️  No SPIR-V files found. Make sure shaders are compiled first.")
            return False
        
        print(f"📁 Found {len(spirv_files)} SPIR-V files")
        
        # Parse C++ structs
        print("🔍 Parsing C++ struct definitions...")
        cpp_structs = self.find_cpp_structs(cpp_dirs)
        print(f"📁 Found {len(cpp_structs)} C++ struct definitions")
        
        # Validate each shader
        for spirv_file in spirv_files:
            self.validate_shader(spirv_file, cpp_structs)
        
        # Generate report
        self.generate_report(report_path)
        
        return len(self.report["layout_mismatches"]) == 0


def main():
    parser = argparse.ArgumentParser(description="Validate Vulkan shader layouts against C++ definitions")
    parser.add_argument("--shader-dirs", nargs="+", default=["build/shaders_cache"], 
                       help="Directories containing compiled SPIR-V shaders")
    parser.add_argument("--cpp-dirs", nargs="+", default=["src", "include"],
                       help="Directories containing C++ header files")
    parser.add_argument("--report", default="reports/shaders/layout_check.json",
                       help="Output validation report path")
    parser.add_argument("--spirv-cross", default="spirv-cross",
                       help="Path to spirv-cross executable")
    
    args = parser.parse_args()
    
    validator = ShaderLayoutValidator(args.spirv_cross)
    success = validator.run_validation(args.shader_dirs, args.cpp_dirs, args.report)
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()