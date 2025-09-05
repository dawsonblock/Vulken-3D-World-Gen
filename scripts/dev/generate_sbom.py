#!/usr/bin/env python3
"""
Generate Software Bill of Materials (SBOM) in CycloneDX format
"""

import json
import subprocess
import sys
from pathlib import Path
import datetime
import argparse


def get_vcpkg_packages():
    """Extract installed vcpkg packages"""
    packages = []
    try:
        # Try to read vcpkg.json
        vcpkg_json = Path('vcpkg.json')
        if vcpkg_json.exists():
            with open(vcpkg_json) as f:
                data = json.load(f)
                deps = data.get('dependencies', [])
                for dep in deps:
                    if isinstance(dep, str):
                        packages.append({'name': dep, 'version': 'latest'})
                    elif isinstance(dep, dict):
                        packages.append({'name': dep.get('name', 'unknown'), 'version': 'latest'})
    except Exception as e:
        print(f"Warning: Could not read vcpkg dependencies: {e}")
    
    return packages


def get_git_info():
    """Get git repository information"""
    try:
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
        branch = subprocess.check_output(['git', 'branch', '--show-current'], text=True).strip()
        return {'commit': commit, 'branch': branch}
    except:
        return {'commit': 'unknown', 'branch': 'unknown'}


def generate_sbom(output_path):
    """Generate CycloneDX SBOM"""
    git_info = get_git_info()
    packages = get_vcpkg_packages()
    
    sbom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.4",
        "serialNumber": f"urn:uuid:voxelvk-{datetime.datetime.now().isoformat()}",
        "version": 1,
        "metadata": {
            "timestamp": datetime.datetime.now().isoformat(),
            "tools": [
                {
                    "vendor": "VoxelVK",
                    "name": "sbom-generator",
                    "version": "1.0.0"
                }
            ],
            "component": {
                "type": "application",
                "name": "VoxelVK",
                "version": f"git-{git_info['commit'][:8]}",
                "description": "High-performance voxel engine with Vulkan rendering"
            }
        },
        "components": []
    }
    
    # Add vcpkg dependencies
    for pkg in packages:
        sbom["components"].append({
            "type": "library",
            "name": pkg['name'],
            "version": pkg['version'],
            "scope": "required",
            "purl": f"pkg:vcpkg/{pkg['name']}@{pkg['version']}"
        })
    
    # Write SBOM
    output_dir = Path(output_path).parent
    output_dir.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        json.dump(sbom, f, indent=2)
    
    print(f"SBOM generated: {output_path}")
    print(f"Components: {len(sbom['components'])}")
    

def main():
    parser = argparse.ArgumentParser(description='Generate SBOM for VoxelVK')
    parser.add_argument('--output', '-o', default='sbom.json', help='Output SBOM file')
    
    args = parser.parse_args()
    generate_sbom(args.output)


if __name__ == '__main__':
    main()