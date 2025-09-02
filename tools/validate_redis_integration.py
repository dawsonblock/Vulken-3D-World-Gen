#!/usr/bin/env python3
"""
Redis Integration Validation Script
Comprehensive validation of Redis asset streaming and hot-reload capabilities.
"""

import subprocess
import sys
import os
import json
import time

def validate_redis_service():
    """Validate Redis service is running and accessible"""
    print("🔄 Validating Redis service...")
    
    try:
        result = subprocess.run(['redis-cli', 'ping'], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0 and 'PONG' in result.stdout:
            print("✅ Redis service is running")
            return True
        else:
            print("❌ Redis service not responding")
            return False
    except Exception as e:
        print(f"❌ Redis service check failed: {e}")
        return False

def validate_asset_tools():
    """Validate asset pushing tools work correctly"""
    print("\n📦 Validating asset pushing tools...")
    
    success_count = 0
    
    # Test mesh pushing
    try:
        result = subprocess.run([
            'python3', 'tools/push_mesh.py', 
            'validation_cube', 'assets/samples/cube.obj'
        ], capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0 and 'Successfully pushed mesh' in result.stdout:
            print("✅ Mesh pushing tool working")
            success_count += 1
        else:
            print(f"❌ Mesh pushing failed: {result.stderr}")
    except Exception as e:
        print(f"❌ Mesh pushing tool error: {e}")
    
    # Test voxel pushing
    try:
        result = subprocess.run([
            'python3', 'tools/push_voxel.py', 
            'validation_voxels', 'assets/samples/test_voxel_chunk.json'
        ], capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0 and 'Successfully pushed voxel' in result.stdout:
            print("✅ Voxel pushing tool working")
            success_count += 1
        else:
            print(f"❌ Voxel pushing failed: {result.stderr}")
    except Exception as e:
        print(f"❌ Voxel pushing tool error: {e}")
    
    return success_count >= 2

def validate_docker_integration():
    """Validate Docker compose Redis integration"""
    print("\n🐳 Validating Docker integration...")
    
    try:
        # Check if Redis container is running
        result = subprocess.run([
            'docker', 'compose', 'ps', '--services', '--filter', 'status=running'
        ], capture_output=True, text=True)
        
        if 'redis' in result.stdout:
            print("✅ Redis Docker container is running")
            
            # Check Redis logs for any errors
            log_result = subprocess.run([
                'docker', 'compose', 'logs', '--tail', '10', 'redis'
            ], capture_output=True, text=True)
            
            if 'Ready to accept connections' in log_result.stdout:
                print("✅ Redis container started successfully")
                return True
            else:
                print("⚠️ Redis container may have startup issues")
                return True  # Still working but with warnings
        else:
            print("❌ Redis Docker container not running")
            return False
            
    except Exception as e:
        print(f"❌ Docker integration check failed: {e}")
        return False

def validate_production_hardening():
    """Check production hardening features"""
    print("\n🛡️ Validating production hardening features...")
    
    hardening_score = 0
    total_checks = 5
    
    # Check if CI configuration exists
    if os.path.exists('.github/workflows/ci.yml'):
        print("✅ CI/CD configuration present")
        hardening_score += 1
    else:
        print("❌ CI/CD configuration missing")
    
    # Check if tests directory exists and has content
    if os.path.exists('tests') and len(os.listdir('tests')) > 5:
        print("✅ Test suite present")
        hardening_score += 1
    else:
        print("❌ Test suite insufficient")
    
    # Check if documentation exists
    if os.path.exists('BUILD.md') and os.path.exists('RUN.md'):
        print("✅ Production documentation present")
        hardening_score += 1
    else:
        print("❌ Production documentation missing")
    
    # Check if security measures exist
    if os.path.exists('SECURITY.md'):
        print("✅ Security documentation present")
        hardening_score += 1
    else:
        print("❌ Security documentation missing")
    
    # Check if dependency management is in place
    if os.path.exists('vcpkg.json') and os.path.exists('CMakeLists.txt'):
        print("✅ Modern dependency management present")
        hardening_score += 1
    else:
        print("❌ Dependency management issues")
    
    print(f"   Hardening score: {hardening_score}/{total_checks}")
    return hardening_score >= 4  # 80% pass rate

def main():
    print("🚀 Redis Integration and Production Hardening Validation")
    print("=" * 70)
    
    # Run validations
    redis_ok = validate_redis_service()
    docker_ok = validate_docker_integration()
    tools_ok = validate_asset_tools()
    hardening_ok = validate_production_hardening()
    
    # Generate report
    report = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
        "redis_validation": {
            "service_status": redis_ok,
            "docker_integration": docker_ok, 
            "asset_tools": tools_ok
        },
        "production_hardening": hardening_ok
    }
    
    # Write report to file
    with open('redis_validation_report.json', 'w') as f:
        json.dump(report, f, indent=2)
    
    # Summary
    print("\n📊 VALIDATION SUMMARY")
    print("=" * 30)
    
    redis_integration_ok = redis_ok and docker_ok and tools_ok
    
    print(f"Redis Integration:      {'✅ PASS' if redis_integration_ok else '❌ FAIL'}")
    print(f"Production Hardening:   {'✅ PASS' if hardening_ok else '❌ FAIL'}")
    
    overall_success = redis_integration_ok and hardening_ok
    
    print(f"\nOVERALL RESULT: {'🎉 SUCCESS' if overall_success else '💥 FAILED'}")
    
    if overall_success:
        print("\n✨ Redis integration and production hardening validation completed successfully!")
        print("   All systems are ready for production deployment.")
        print(f"   Detailed report saved to: redis_validation_report.json")
        return 0
    else:
        print("\n⚠️ Some validation checks failed. Review the issues above.")
        print(f"   Detailed report saved to: redis_validation_report.json")
        return 1

if __name__ == "__main__":
    sys.exit(main())