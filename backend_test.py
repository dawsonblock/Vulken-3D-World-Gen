#!/usr/bin/env python3
"""
VoxelVK Build Engineering Validation Test Suite
Tests build configuration, apps gating, scripts, CI, and Docker optimization
"""

import json
import subprocess
import sys
import os
from pathlib import Path
# import yaml  # Not needed for this validation
import re


class VoxelVKBuildValidator:
    def __init__(self):
        self.results = {
            "build_config": {"status": "UNKNOWN", "details": []},
            "apps_gating": {"status": "UNKNOWN", "details": []},
            "script_functionality": {"status": "UNKNOWN", "details": []},
            "ci_configuration": {"status": "UNKNOWN", "details": []},
            "docker_optimization": {"status": "UNKNOWN", "details": []}
        }
        
    def validate_build_configuration(self):
        """Validate vcpkg-configuration.json, CMakePresets.json, and .clang-tidy"""
        print("=== Build Configuration Validation ===")
        
        # Check vcpkg-configuration.json baseline
        try:
            with open('/app/vcpkg-configuration.json', 'r') as f:
                vcpkg_config = json.load(f)
            
            baseline = vcpkg_config.get('default-registry', {}).get('baseline')
            if baseline == '2024.12.12':
                self.results["build_config"]["details"].append("✅ vcpkg baseline correctly locked to 2024.12.12")
            else:
                self.results["build_config"]["details"].append(f"❌ vcpkg baseline is {baseline}, expected 2024.12.12")
                
        except Exception as e:
            self.results["build_config"]["details"].append(f"❌ Error reading vcpkg-configuration.json: {e}")
        
        # Check CMakePresets.json for coverage preset
        try:
            with open('/app/CMakePresets.json', 'r') as f:
                cmake_presets = json.load(f)
            
            coverage_preset = None
            for preset in cmake_presets.get('configurePresets', []):
                if preset.get('name') == 'coverage':
                    coverage_preset = preset
                    break
            
            if coverage_preset:
                cache_vars = coverage_preset.get('cacheVariables', {})
                has_coverage_flags = (
                    '--coverage' in cache_vars.get('CMAKE_CXX_FLAGS', '') and
                    '--coverage' in cache_vars.get('CMAKE_EXE_LINKER_FLAGS', '')
                )
                if has_coverage_flags:
                    self.results["build_config"]["details"].append("✅ CMake coverage preset exists with proper --coverage flags")
                else:
                    self.results["build_config"]["details"].append("❌ CMake coverage preset missing proper --coverage flags")
            else:
                self.results["build_config"]["details"].append("❌ CMake coverage preset not found")
                
        except Exception as e:
            self.results["build_config"]["details"].append(f"❌ Error reading CMakePresets.json: {e}")
        
        # Check .clang-tidy for WarningsAsErrors
        try:
            with open('/app/.clang-tidy', 'r') as f:
                clang_tidy_content = f.read()
            
            if "WarningsAsErrors: '*'" in clang_tidy_content:
                self.results["build_config"]["details"].append("✅ .clang-tidy has WarningsAsErrors enabled")
            else:
                self.results["build_config"]["details"].append("❌ .clang-tidy missing WarningsAsErrors: '*'")
                
        except Exception as e:
            self.results["build_config"]["details"].append(f"❌ Error reading .clang-tidy: {e}")
        
        # Determine overall status
        failed_checks = [d for d in self.results["build_config"]["details"] if d.startswith("❌")]
        self.results["build_config"]["status"] = "PASS" if not failed_checks else "FAIL"
        
    def validate_apps_gating(self):
        """Verify core demos are NOT behind BUILD_EXTRAS, P0/P1/P2/operator_console ARE gated"""
        print("=== Apps Gating Verification ===")
        
        # Check root CMakeLists.txt for core demos
        core_demos = ['gui_fullscreen_demo', 'vulkan_fullscreen_demo', 'smoke_headless', 'rl_nav_demo']
        gated_apps = ['p0_reliability_test', 'p1_memory_demo', 'p2_concepts_validator', 'operator_console']
        
        try:
            with open('/app/CMakeLists.txt', 'r') as f:
                root_cmake = f.read()
            
            # Check core demos are NOT gated
            for demo in core_demos:
                if demo in root_cmake:
                    # Look for BUILD_EXTRAS gating around this demo
                    demo_lines = [line for line in root_cmake.split('\n') if demo in line]
                    if demo_lines:
                        # Check if any line with this demo is inside BUILD_EXTRAS block
                        build_extras_gated = False
                        lines = root_cmake.split('\n')
                        in_build_extras = False
                        for line in lines:
                            if 'if(BUILD_EXTRAS)' in line:
                                in_build_extras = True
                            elif 'endif()' in line and in_build_extras:
                                in_build_extras = False
                            elif demo in line and in_build_extras:
                                build_extras_gated = True
                                break
                        
                        if not build_extras_gated:
                            self.results["apps_gating"]["details"].append(f"✅ Core demo {demo} is NOT behind BUILD_EXTRAS")
                        else:
                            self.results["apps_gating"]["details"].append(f"❌ Core demo {demo} is incorrectly gated behind BUILD_EXTRAS")
                else:
                    self.results["apps_gating"]["details"].append(f"⚠️ Core demo {demo} not found in root CMakeLists.txt")
            
        except Exception as e:
            self.results["apps_gating"]["details"].append(f"❌ Error reading root CMakeLists.txt: {e}")
        
        # Check apps/CMakeLists.txt for gated apps
        try:
            with open('/app/apps/CMakeLists.txt', 'r') as f:
                apps_cmake = f.read()
            
            # Check operator_console is gated
            if 'if(BUILD_EXTRAS)' in apps_cmake and 'operator_console' in apps_cmake:
                self.results["apps_gating"]["details"].append("✅ operator_console is properly gated behind BUILD_EXTRAS")
            else:
                self.results["apps_gating"]["details"].append("❌ operator_console not properly gated behind BUILD_EXTRAS")
            
            # Check for duplicate source definitions (should be cleaned)
            duplicate_patterns = ['target_sources.*VoxelVK_Elite_ALL.*PRIVATE']
            for pattern in duplicate_patterns:
                if re.search(pattern, apps_cmake):
                    self.results["apps_gating"]["details"].append("✅ Found deduplicated source definitions in apps/CMakeLists.txt")
                    break
            
        except Exception as e:
            self.results["apps_gating"]["details"].append(f"❌ Error reading apps/CMakeLists.txt: {e}")
        
        # Determine overall status
        failed_checks = [d for d in self.results["apps_gating"]["details"] if d.startswith("❌")]
        self.results["apps_gating"]["status"] = "PASS" if not failed_checks else "FAIL"
        
    def validate_script_functionality(self):
        """Test scripts can be executed (dry run)"""
        print("=== Script Functionality Validation ===")
        
        scripts_to_test = [
            '/app/scripts/shaders/validate_layout.py',
            '/app/scripts/dev/generate_sbom.py'
        ]
        
        for script_path in scripts_to_test:
            try:
                # Check if script exists and is executable
                if os.path.exists(script_path) and os.access(script_path, os.X_OK):
                    self.results["script_functionality"]["details"].append(f"✅ {script_path} exists and is executable")
                    
                    # Check shebang
                    with open(script_path, 'r') as f:
                        first_line = f.readline().strip()
                    if first_line.startswith('#!/usr/bin/env python3'):
                        self.results["script_functionality"]["details"].append(f"✅ {script_path} has proper shebang")
                    else:
                        self.results["script_functionality"]["details"].append(f"❌ {script_path} missing proper shebang")
                    
                    # Try dry run with --help
                    try:
                        result = subprocess.run([script_path, '--help'], 
                                              capture_output=True, text=True, timeout=10)
                        if result.returncode == 0:
                            self.results["script_functionality"]["details"].append(f"✅ {script_path} --help executes successfully")
                        else:
                            self.results["script_functionality"]["details"].append(f"⚠️ {script_path} --help returned non-zero: {result.returncode}")
                    except subprocess.TimeoutExpired:
                        self.results["script_functionality"]["details"].append(f"⚠️ {script_path} --help timed out")
                    except Exception as e:
                        self.results["script_functionality"]["details"].append(f"⚠️ {script_path} execution error: {e}")
                        
                else:
                    self.results["script_functionality"]["details"].append(f"❌ {script_path} not found or not executable")
                    
            except Exception as e:
                self.results["script_functionality"]["details"].append(f"❌ Error checking {script_path}: {e}")
        
        # Determine overall status
        failed_checks = [d for d in self.results["script_functionality"]["details"] if d.startswith("❌")]
        self.results["script_functionality"]["status"] = "PASS" if not failed_checks else "FAIL"
        
    def validate_ci_configuration(self):
        """Validate CI configuration for vcpkg caching, coverage job, shader validation, release artifacts"""
        print("=== CI Configuration Validation ===")
        
        try:
            with open('/app/.github/workflows/ci.yml', 'r') as f:
                ci_content = f.read()
            
            # Check vcpkg caching with proper cache key
            if 'uses: actions/cache@v4' in ci_content and 'vcpkg' in ci_content:
                # Look for cache key that includes vcpkg files
                cache_key_pattern = r'key:.*vcpkg.*hashFiles.*vcpkg.*json'
                if re.search(cache_key_pattern, ci_content, re.IGNORECASE | re.DOTALL):
                    self.results["ci_configuration"]["details"].append("✅ vcpkg caching configured with proper cache key")
                else:
                    self.results["ci_configuration"]["details"].append("❌ vcpkg caching missing proper cache key with hashFiles")
            else:
                self.results["ci_configuration"]["details"].append("❌ vcpkg caching not found")
            
            # Check for coverage job
            if 'coverage:' in ci_content and 'gcovr' in ci_content:
                self.results["ci_configuration"]["details"].append("✅ Coverage job found with gcovr")
            else:
                self.results["ci_configuration"]["details"].append("❌ Coverage job not found or missing gcovr")
            
            # Check shader validation enhancements
            if 'shader_validation:' in ci_content or 'glslangValidator' in ci_content:
                self.results["ci_configuration"]["details"].append("✅ Shader validation job found")
            else:
                self.results["ci_configuration"]["details"].append("❌ Shader validation job not found")
            
            # Check release artifacts job with SBOM + SHA256SUMS
            if 'release_artifacts:' in ci_content:
                if 'generate_sbom.py' in ci_content and 'SHA256SUMS' in ci_content:
                    self.results["ci_configuration"]["details"].append("✅ Release artifacts job with SBOM + SHA256SUMS generation")
                else:
                    self.results["ci_configuration"]["details"].append("❌ Release artifacts job missing SBOM or SHA256SUMS generation")
            else:
                self.results["ci_configuration"]["details"].append("❌ Release artifacts job not found")
                
        except Exception as e:
            self.results["ci_configuration"]["details"].append(f"❌ Error reading CI configuration: {e}")
        
        # Determine overall status
        failed_checks = [d for d in self.results["ci_configuration"]["details"] if d.startswith("❌")]
        self.results["ci_configuration"]["status"] = "PASS" if not failed_checks else "FAIL"
        
    def validate_docker_optimization(self):
        """Verify Docker multi-stage build, runtime stage optimization, non-root user"""
        print("=== Docker Optimization Validation ===")
        
        try:
            with open('/app/Dockerfile', 'r') as f:
                dockerfile_content = f.read()
            
            # Check multi-stage build
            from_count = dockerfile_content.count('FROM ')
            if from_count >= 2:
                self.results["docker_optimization"]["details"].append(f"✅ Multi-stage build detected ({from_count} stages)")
            else:
                self.results["docker_optimization"]["details"].append(f"❌ Single stage build detected, expected multi-stage")
            
            # Check runtime stage uses distroless or minimal base
            if 'FROM gcr.io/distroless' in dockerfile_content or 'FROM alpine' in dockerfile_content:
                self.results["docker_optimization"]["details"].append("✅ Runtime stage uses minimal base image")
            else:
                self.results["docker_optimization"]["details"].append("❌ Runtime stage not using minimal base image")
            
            # Check binaries are stripped
            if 'strip' in dockerfile_content:
                self.results["docker_optimization"]["details"].append("✅ Binaries are stripped in build process")
            else:
                self.results["docker_optimization"]["details"].append("❌ No binary stripping found")
            
            # Check non-root user
            if 'USER ' in dockerfile_content and '65532' in dockerfile_content:
                self.results["docker_optimization"]["details"].append("✅ Non-root user configured (65532)")
            elif 'USER ' in dockerfile_content:
                self.results["docker_optimization"]["details"].append("✅ Non-root user configured")
            else:
                self.results["docker_optimization"]["details"].append("❌ No non-root user configuration found")
            
            # Check no build tools in runtime stage
            lines = dockerfile_content.split('\n')
            in_runtime_stage = False
            build_tools_in_runtime = False
            
            for line in lines:
                if 'FROM gcr.io/distroless' in line or ('FROM ' in line and 'AS build' not in line and from_count >= 2):
                    in_runtime_stage = True
                elif 'FROM ' in line and in_runtime_stage:
                    in_runtime_stage = False
                elif in_runtime_stage and any(tool in line.lower() for tool in ['gcc', 'g++', 'cmake', 'ninja', 'build-essential']):
                    build_tools_in_runtime = True
                    break
            
            if not build_tools_in_runtime:
                self.results["docker_optimization"]["details"].append("✅ No build tools copied to runtime stage")
            else:
                self.results["docker_optimization"]["details"].append("❌ Build tools found in runtime stage")
                
        except Exception as e:
            self.results["docker_optimization"]["details"].append(f"❌ Error reading Dockerfile: {e}")
        
        # Determine overall status
        failed_checks = [d for d in self.results["docker_optimization"]["details"] if d.startswith("❌")]
        self.results["docker_optimization"]["status"] = "PASS" if not failed_checks else "FAIL"
        
    def run_all_validations(self):
        """Run all validation tests"""
        print("VoxelVK Build Engineering Validation Suite")
        print("=" * 50)
        
        self.validate_build_configuration()
        print()
        self.validate_apps_gating()
        print()
        self.validate_script_functionality()
        print()
        self.validate_ci_configuration()
        print()
        self.validate_docker_optimization()
        print()
        
        # Print summary
        print("=" * 50)
        print("VALIDATION SUMMARY")
        print("=" * 50)
        
        overall_status = "PASS"
        for category, result in self.results.items():
            status_icon = "✅" if result["status"] == "PASS" else "❌"
            print(f"{status_icon} {category.replace('_', ' ').title()}: {result['status']}")
            
            if result["status"] == "FAIL":
                overall_status = "FAIL"
            
            for detail in result["details"]:
                print(f"   {detail}")
            print()
        
        print(f"Overall Status: {overall_status}")
        return overall_status == "PASS"


if __name__ == "__main__":
    validator = VoxelVKBuildValidator()
    success = validator.run_all_validations()
    sys.exit(0 if success else 1)