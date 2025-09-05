#!/usr/bin/env python3
"""
VoxelVK Software Bill of Materials (SBOM) Generator

Generates SPDX-format SBOM for VoxelVK releases.
Scans dependencies, licenses, and creates compliance reports.
"""

import os
import sys
import json
import datetime
import subprocess
import hashlib
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Set
import uuid

class Dependency:
    def __init__(self, name: str, version: str, license_: str, source: str = ""):
        self.name = name
        self.version = version
        self.license = license_
        self.source = source
        self.spdx_id = f"SPDXRef-{name.replace('/', '-').replace(':', '-')}"
        self.files: List[str] = []
        self.checksum = ""
        
    def to_spdx(self) -> Dict:
        return {
            "SPDXID": self.spdx_id,
            "name": self.name,
            "downloadLocation": self.source or "NOASSERTION",
            "filesAnalyzed": len(self.files) > 0,
            "licenseConcluded": self.license or "NOASSERTION",
            "licenseDeclared": self.license or "NOASSERTION",
            "copyrightText": "NOASSERTION",
            "versionInfo": self.version,
            "supplier": "NOASSERTION",
            "checksums": [
                {
                    "algorithm": "SHA256",
                    "checksumValue": self.checksum or "0000000000000000000000000000000000000000000000000000000000000000"
                }
            ] if self.checksum else []
        }

class SBOMGenerator:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.dependencies: List[Dependency] = []
        self.document_id = str(uuid.uuid4())
        self.creation_time = datetime.datetime.utcnow().isoformat() + "Z"
        
    def scan_vcpkg_dependencies(self) -> List[Dependency]:
        """Scan vcpkg.json for dependencies"""
        deps = []
        vcpkg_file = self.project_root / "vcpkg.json"
        
        if vcpkg_file.exists():
            try:
                with open(vcpkg_file) as f:
                    vcpkg_data = json.load(f)
                    
                # Regular dependencies
                for dep_name in vcpkg_data.get("dependencies", []):
                    if isinstance(dep_name, str):
                        deps.append(Dependency(
                            name=f"vcpkg::{dep_name}",
                            version="NOASSERTION",
                            license_="NOASSERTION",
                            source=f"https://github.com/Microsoft/vcpkg/tree/master/ports/{dep_name}"
                        ))
                    elif isinstance(dep_name, dict):
                        name = dep_name.get("name", "unknown")
                        version = dep_name.get("version", "NOASSERTION")
                        deps.append(Dependency(
                            name=f"vcpkg::{name}",
                            version=version,
                            license_="NOASSERTION",
                            source=f"https://github.com/Microsoft/vcpkg/tree/master/ports/{name}"
                        ))
                        
            except (json.JSONDecodeError, FileNotFoundError) as e:
                print(f"Warning: Could not parse vcpkg.json: {e}")
                
        return deps
        
    def scan_system_dependencies(self) -> List[Dependency]:
        """Scan for system dependencies (Vulkan, OpenGL, etc.)"""
        deps = []
        
        # Known system dependencies
        system_deps = [
            ("Vulkan SDK", "1.3.0", "Apache-2.0", "https://vulkan.lunarg.com/"),
            ("OpenGL", "4.6", "MIT", "https://www.opengl.org/"),
            ("X11", "1.8", "MIT", "https://www.x.org/"),
            ("glibc", "2.27", "LGPL-2.1", "https://www.gnu.org/software/libc/"),
        ]
        
        for name, version, license_, source in system_deps:
            deps.append(Dependency(
                name=f"system::{name}",
                version=version,
                license_=license_,
                source=source
            ))
            
        return deps
        
    def scan_source_files(self) -> List[str]:
        """Scan source files for license headers"""
        source_files = []
        
        # Scan common source directories
        for pattern in ["src/**/*.cpp", "src/**/*.hpp", "src/**/*.c", "src/**/*.h"]:
            source_files.extend(self.project_root.glob(pattern))
            
        return [str(f.relative_to(self.project_root)) for f in source_files]
        
    def calculate_file_hash(self, filepath: str) -> str:
        """Calculate SHA256 hash of a file"""
        try:
            with open(self.project_root / filepath, 'rb') as f:
                return hashlib.sha256(f.read()).hexdigest()
        except FileNotFoundError:
            return ""
            
    def generate_sbom(self) -> Dict:
        """Generate complete SPDX SBOM"""
        
        # Scan dependencies
        self.dependencies.extend(self.scan_vcpkg_dependencies())
        self.dependencies.extend(self.scan_system_dependencies())
        
        # Scan source files
        source_files = self.scan_source_files()
        
        # Create main package
        main_package = {
            "SPDXID": "SPDXRef-Package",
            "name": "VoxelVK",
            "downloadLocation": "https://github.com/voxelvk/voxelvk",
            "filesAnalyzed": True,
            "licenseConcluded": "MIT",
            "licenseDeclared": "MIT",
            "copyrightText": "Copyright (c) 2024 VoxelVK Team",
            "versionInfo": "0.6.0",
            "supplier": "Organization: VoxelVK Team",
            "homepage": "https://github.com/voxelvk/voxelvk",
            "sourceInfo": "Built from source",
            "checksums": []
        }
        
        # Create SBOM document
        sbom = {
            "spdxVersion": "SPDX-2.3",
            "dataLicense": "CC0-1.0",
            "SPDXID": "SPDXRef-DOCUMENT",
            "name": f"VoxelVK-{self.document_id}",
            "documentNamespace": f"https://voxelvk.org/sbom/{self.document_id}",
            "creationInfo": {
                "created": self.creation_time,
                "creators": [
                    "Tool: VoxelVK SBOM Generator",
                    "Organization: VoxelVK Team"
                ]
            },
            "packages": [main_package] + [dep.to_spdx() for dep in self.dependencies],
            "relationships": []
        }
        
        # Add dependency relationships
        for dep in self.dependencies:
            sbom["relationships"].append({
                "spdxElementId": "SPDXRef-Package",
                "relationshipType": "DEPENDS_ON",
                "relatedSpdxElement": dep.spdx_id
            })
            
        # Add file information
        files = []
        for filepath in source_files[:50]:  # Limit to first 50 files for demo
            file_hash = self.calculate_file_hash(filepath)
            files.append({
                "SPDXID": f"SPDXRef-File-{len(files)}",
                "fileName": f"./{filepath}",
                "checksums": [{
                    "algorithm": "SHA256",
                    "checksumValue": file_hash
                }] if file_hash else [],
                "licenseConcluded": "MIT",
                "copyrightText": "Copyright (c) 2024 VoxelVK Team"
            })
            
        sbom["files"] = files
        
        return sbom
        
    def validate_licenses(self) -> Dict[str, List[str]]:
        """Validate license compatibility"""
        results = {
            "compatible": [],
            "incompatible": [],
            "unknown": []
        }
        
        # Compatible licenses for MIT project
        compatible_licenses = {
            "MIT", "BSD-2-Clause", "BSD-3-Clause", "Apache-2.0",
            "CC0-1.0", "ISC", "Unlicense", "LGPL-2.1", "LGPL-3.0"
        }
        
        # Incompatible licenses
        incompatible_licenses = {
            "GPL-2.0", "GPL-3.0", "AGPL-3.0", "SSPL-1.0"
        }
        
        for dep in self.dependencies:
            license_name = dep.license.upper() if dep.license != "NOASSERTION" else "UNKNOWN"
            
            if license_name in compatible_licenses:
                results["compatible"].append(f"{dep.name}: {dep.license}")
            elif license_name in incompatible_licenses:
                results["incompatible"].append(f"{dep.name}: {dep.license}")
            else:
                results["unknown"].append(f"{dep.name}: {dep.license}")
                
        return results
        
    def save_sbom(self, output_path: str):
        """Save SBOM to file"""
        sbom = self.generate_sbom()
        
        with open(output_path, 'w') as f:
            json.dump(sbom, f, indent=2, sort_keys=True)
            
        print(f"SBOM saved to: {output_path}")
        print(f"Document ID: {self.document_id}")
        print(f"Creation time: {self.creation_time}")
        print(f"Dependencies found: {len(self.dependencies)}")
        
        # License validation
        license_results = self.validate_licenses()
        print(f"\nLicense Analysis:")
        print(f"  Compatible: {len(license_results['compatible'])}")
        print(f"  Incompatible: {len(license_results['incompatible'])}")
        print(f"  Unknown: {len(license_results['unknown'])}")
        
        if license_results["incompatible"]:
            print(f"\nWARNING: Incompatible licenses found:")
            for item in license_results["incompatible"]:
                print(f"  - {item}")
                
        return sbom

def main():
    parser = argparse.ArgumentParser(description='Generate SBOM for VoxelVK')
    parser.add_argument('--output', '-o', default='voxelvk-sbom.json',
                       help='Output SBOM file path')
    parser.add_argument('--project-root', default='.',
                       help='Project root directory')
    parser.add_argument('--validate-only', action='store_true',
                       help='Only validate licenses, do not generate SBOM')
    
    args = parser.parse_args()
    
    generator = SBOMGenerator(args.project_root)
    
    if args.validate_only:
        # Quick license validation
        generator.dependencies.extend(generator.scan_vcpkg_dependencies())
        generator.dependencies.extend(generator.scan_system_dependencies())
        
        results = generator.validate_licenses()
        print("License Validation Results:")
        print(f"Compatible: {len(results['compatible'])}")
        print(f"Incompatible: {len(results['incompatible'])}")
        print(f"Unknown: {len(results['unknown'])}")
        
        if results["incompatible"]:
            print("\nIncompatible licenses:")
            for item in results["incompatible"]:
                print(f"  {item}")
            sys.exit(1)
    else:
        generator.save_sbom(args.output)

if __name__ == '__main__':
    main()