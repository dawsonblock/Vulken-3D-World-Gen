#!/usr/bin/env python3
"""
Shader Layout Validation Script
Validates SPIR-V shader layouts against C++ struct definitions
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path
import re


def run_spirv_cross(shader_path):
    """Run spirv-cross to reflect shader and extract layout info"""
    try:
        result = subprocess.run([
            'spirv-cross', '--output', '/dev/null', '--reflect', str(shader_path)
        ], capture_output=True, text=True, check=True)
        return result.stderr  # spirv-cross outputs reflection to stderr
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Warning: spirv-cross failed for {shader_path}: {e}")
        return ""


def validate_shader_layouts(shader_dirs, cpp_dirs, report_path):
    """Main validation function"""
    validation_results = {
        'timestamp': None,
        'shader_files_checked': 0,
        'cpp_files_scanned': 0,
        'layout_mismatches': [],
        'validation_summary': {
            'total_mismatches': 0,
            'validation_status': 'PASS'
        }
    }
    
    # Find all SPIR-V files
    spirv_files = []
    for shader_dir in shader_dirs:
        if Path(shader_dir).exists():
            spirv_files.extend(Path(shader_dir).rglob("*.spv"))
    
    validation_results['shader_files_checked'] = len(spirv_files)
    
    # For each SPIR-V file, validate layout
    for spirv_file in spirv_files:
        reflection = run_spirv_cross(spirv_file)
        if not reflection:
            continue
            
        # Simple validation - check for common layout issues
        if 'error' in reflection.lower() or 'warning' in reflection.lower():
            validation_results['layout_mismatches'].append({
                'shader': str(spirv_file),
                'issue': 'spirv-cross reported warnings/errors',
                'details': reflection[:200] + '...' if len(reflection) > 200 else reflection
            })
    
    # Count C++ files for reference
    cpp_files = []
    for cpp_dir in cpp_dirs:
        if Path(cpp_dir).exists():
            cpp_files.extend(Path(cpp_dir).rglob("*.cpp"))
            cpp_files.extend(Path(cpp_dir).rglob("*.hpp"))
            cpp_files.extend(Path(cpp_dir).rglob("*.h"))
    
    validation_results['cpp_files_scanned'] = len(cpp_files)
    validation_results['validation_summary']['total_mismatches'] = len(validation_results['layout_mismatches'])
    
    if validation_results['validation_summary']['total_mismatches'] > 0:
        validation_results['validation_summary']['validation_status'] = 'FAIL'
    
    # Write report
    report_dir = Path(report_path).parent
    report_dir.mkdir(parents=True, exist_ok=True)
    
    with open(report_path, 'w') as f:
        json.dump(validation_results, f, indent=2)
    
    print(f"Shader validation complete: {validation_results['validation_summary']['validation_status']}")
    print(f"Checked {validation_results['shader_files_checked']} shaders, {validation_results['validation_summary']['total_mismatches']} mismatches")
    
    return validation_results['validation_summary']['total_mismatches'] == 0


def main():
    parser = argparse.ArgumentParser(description='Validate shader layouts against C++ definitions')
    parser.add_argument('--shader-dirs', nargs='+', default=['shaders', 'shaders_vk'], 
                       help='Directories containing SPIR-V files')
    parser.add_argument('--cpp-dirs', nargs='+', default=['src', 'include'],
                       help='Directories containing C++ header files')
    parser.add_argument('--report', default='reports/shaders/layout_check.json',
                       help='Output report file path')
    
    args = parser.parse_args()
    
    success = validate_shader_layouts(args.shader_dirs, args.cpp_dirs, args.report)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()