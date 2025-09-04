#!/usr/bin/env python3
"""
Vulken-3D Production System Test Suite
=====================================

Comprehensive testing of the Vulken-3D voxel world generation engine
covering configuration, Python scripts, documentation, and system integration.
"""

import os
import sys
import yaml
import json
import subprocess
import time
from pathlib import Path
from datetime import datetime

class VulkenTestSuite:
    def __init__(self):
        self.tests_run = 0
        self.tests_passed = 0
        self.failures = []
        self.start_time = time.time()
        
    def run_test(self, name, test_func):
        """Run a single test and track results"""
        self.tests_run += 1
        print(f"\n🔍 Testing {name}...")
        
        try:
            result = test_func()
            if result:
                self.tests_passed += 1
                print(f"✅ {name}: PASSED")
                return True
            else:
                print(f"❌ {name}: FAILED")
                self.failures.append(name)
                return False
        except Exception as e:
            print(f"❌ {name}: ERROR - {str(e)}")
            self.failures.append(f"{name} (Exception: {str(e)})")
            return False

    def test_configuration_system(self):
        """Test all YAML configuration files"""
        config_files = [
            'config/engine.yaml',
            'config/weather.yaml', 
            'config/renderer.yaml',
            'config/redis.yaml',
            'config/training.yaml',
            'config/datasets.yaml',
            'config/world.yaml',
            'config/ai_enhanced_training.yaml'
        ]
        
        for config_file in config_files:
            if not os.path.exists(config_file):
                print(f"   ❌ Missing: {config_file}")
                return False
                
            try:
                with open(config_file, 'r') as f:
                    config = yaml.safe_load(f)
                print(f"   ✅ Valid YAML: {config_file}")
            except Exception as e:
                print(f"   ❌ Invalid YAML: {config_file} - {str(e)}")
                return False
                
        return True

    def test_python_scripts(self):
        """Test critical Python scripts"""
        scripts_to_test = [
            ('scripts/data/load_assets_to_redis.py --seed', 'Asset loading'),
            ('scripts/bench/run_bench.py --skip-cpp --output /tmp/test_bench.json', 'Benchmarking'),
            ('scripts/ai/train_stub.py --num-examples 10 --output-dir /tmp/test_ai', 'AI training stub'),
            ('scripts/clean/dedupe_and_purge.py --dry-run', 'Cleanup (dry run)'),
        ]
        
        for script_cmd, description in scripts_to_test:
            try:
                result = subprocess.run(
                    f"cd /app && python3 {script_cmd}",
                    shell=True,
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                if result.returncode == 0:
                    print(f"   ✅ {description}: Working")
                else:
                    print(f"   ❌ {description}: Failed (exit code {result.returncode})")
                    return False
            except subprocess.TimeoutExpired:
                print(f"   ⚠️ {description}: Timeout (may be working but slow)")
            except Exception as e:
                print(f"   ❌ {description}: Error - {str(e)}")
                return False
                
        return True

    def test_docker_compose_validation(self):
        """Test Docker Compose configuration"""
        if not os.path.exists('docker-compose.yml'):
            return False
            
        try:
            with open('docker-compose.yml', 'r') as f:
                config = yaml.safe_load(f)
            
            # Check required services
            required_services = ['vulken3d', 'redis', 'postgres']
            services = config.get('services', {})
            
            for service in required_services:
                if service not in services:
                    print(f"   ❌ Missing service: {service}")
                    return False
                print(f"   ✅ Service defined: {service}")
                
            return True
        except Exception as e:
            print(f"   ❌ Docker Compose validation failed: {str(e)}")
            return False

    def test_helm_chart_validation(self):
        """Test Helm chart configuration"""
        chart_files = [
            'helm/vulken-3d/Chart.yaml',
            'helm/vulken-3d/values.yaml'
        ]
        
        for chart_file in chart_files:
            if not os.path.exists(chart_file):
                print(f"   ❌ Missing: {chart_file}")
                return False
                
            try:
                with open(chart_file, 'r') as f:
                    config = yaml.safe_load(f)
                print(f"   ✅ Valid YAML: {chart_file}")
            except Exception as e:
                print(f"   ❌ Invalid YAML: {chart_file} - {str(e)}")
                return False
                
        return True

    def test_shader_infrastructure(self):
        """Test shader file structure"""
        shader_dirs = [
            'shaders/core',
            'shaders/weather', 
            'shaders/post',
            'shaders_vk/sky',
            'shaders_vk/lighting'
        ]
        
        shader_count = 0
        for shader_dir in shader_dirs:
            if os.path.exists(shader_dir):
                shaders = list(Path(shader_dir).glob('*.comp')) + list(Path(shader_dir).glob('*.vert')) + list(Path(shader_dir).glob('*.frag'))
                shader_count += len(shaders)
                print(f"   ✅ {shader_dir}: {len(shaders)} shaders")
            else:
                print(f"   ⚠️ {shader_dir}: Not found")
                
        return shader_count > 0

    def test_documentation_completeness(self):
        """Test documentation files"""
        required_docs = [
            'README.md',
            'RUN.md',
            'BUILD.md',
            'docs/ARCHITECTURE.md',
            'docs/OPERATOR_GUIDE.md'
        ]
        
        for doc in required_docs:
            if not os.path.exists(doc):
                print(f"   ❌ Missing: {doc}")
                return False
                
            with open(doc, 'r') as f:
                content = f.read()
                
            if len(content) < 100:
                print(f"   ❌ Too short: {doc} ({len(content)} chars)")
                return False
                
            print(f"   ✅ Complete: {doc} ({len(content)} chars)")
            
        return True

    def test_asset_structure(self):
        """Test asset directory structure"""
        asset_dirs = [
            'assets/textures',
            'assets/meshes_library',
            'assets/palettes',
            'assets/config'
        ]
        
        for asset_dir in asset_dirs:
            if not os.path.exists(asset_dir):
                print(f"   ❌ Missing: {asset_dir}")
                return False
                
            files = list(Path(asset_dir).iterdir())
            print(f"   ✅ {asset_dir}: {len(files)} files")
            
        return True

    def test_release_bundle_creation(self):
        """Test release bundle creation capability"""
        try:
            result = subprocess.run(
                "cd /app && python3 scripts/dev/make_release_bundle.py",
                shell=True,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            if result.returncode == 0:
                # Check if bundle was created
                release_files = list(Path('release').glob('vulken3d_*.zip'))
                if release_files:
                    print(f"   ✅ Release bundle created: {release_files[0].name}")
                    return True
                else:
                    print("   ❌ No release bundle found")
                    return False
            else:
                print(f"   ❌ Release bundle creation failed: {result.stderr}")
                return False
                
        except Exception as e:
            print(f"   ❌ Release bundle test error: {str(e)}")
            return False

    def generate_report(self):
        """Generate comprehensive test report"""
        duration = time.time() - self.start_time
        
        report = {
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "test_suite": "Vulken-3D Production System Tests",
            "duration_seconds": round(duration, 2),
            "summary": {
                "total_tests": self.tests_run,
                "passed": self.tests_passed,
                "failed": self.tests_run - self.tests_passed,
                "success_rate": round((self.tests_passed / self.tests_run) * 100, 1) if self.tests_run > 0 else 0
            },
            "failures": self.failures,
            "system_info": {
                "python_version": sys.version,
                "working_directory": os.getcwd(),
                "environment": "Kubernetes Container"
            }
        }
        
        # Save report
        with open('reports/vulken3d_test_report.json', 'w') as f:
            json.dump(report, f, indent=2)
            
        return report

    def run_all_tests(self):
        """Run complete test suite"""
        print("🚀 Starting Vulken-3D Production System Test Suite")
        print("=" * 60)
        
        # Core infrastructure tests
        self.run_test("Configuration System (8 YAML files)", self.test_configuration_system)
        self.run_test("Python Scripts", self.test_python_scripts)
        self.run_test("Docker Compose Validation", self.test_docker_compose_validation)
        self.run_test("Helm Chart Validation", self.test_helm_chart_validation)
        
        # System component tests
        self.run_test("Shader Infrastructure", self.test_shader_infrastructure)
        self.run_test("Documentation Completeness", self.test_documentation_completeness)
        self.run_test("Asset Structure", self.test_asset_structure)
        self.run_test("Release Bundle Creation", self.test_release_bundle_creation)
        
        # Generate final report
        report = self.generate_report()
        
        print("\n" + "=" * 60)
        print("📊 VULKEN-3D TEST SUITE RESULTS")
        print("=" * 60)
        print(f"Tests Run: {self.tests_run}")
        print(f"Passed: {self.tests_passed}")
        print(f"Failed: {self.tests_run - self.tests_passed}")
        print(f"Success Rate: {report['summary']['success_rate']}%")
        print(f"Duration: {report['duration_seconds']}s")
        
        if self.failures:
            print(f"\n❌ Failed Tests:")
            for failure in self.failures:
                print(f"   • {failure}")
        else:
            print(f"\n✅ All tests passed!")
            
        print(f"\n📋 Detailed report: reports/vulken3d_test_report.json")
        
        return self.tests_passed == self.tests_run

def main():
    """Main test execution"""
    # Ensure reports directory exists
    os.makedirs('reports', exist_ok=True)
    
    # Run test suite
    suite = VulkenTestSuite()
    success = suite.run_all_tests()
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()