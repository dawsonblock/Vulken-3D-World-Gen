#!/usr/bin/env python3
"""
VoxelVK Vulnerability Scanner

Scans dependencies for known security vulnerabilities.
Integrates with vulnerability databases and security advisories.
"""

import os
import sys
import json
import argparse
import subprocess
import urllib.request
import urllib.error
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import tempfile
import shutil

@dataclass
class Vulnerability:
    id: str
    severity: str
    title: str
    description: str
    affected_package: str
    affected_version: str
    fixed_version: Optional[str] = None
    cve_id: Optional[str] = None
    cvss_score: Optional[float] = None
    
    def to_dict(self) -> Dict:
        return {
            'id': self.id,
            'severity': self.severity,
            'title': self.title,
            'description': self.description,
            'affected_package': self.affected_package,
            'affected_version': self.affected_version,
            'fixed_version': self.fixed_version,
            'cve_id': self.cve_id,
            'cvss_score': self.cvss_score
        }

class VulnerabilityScanner:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.vulnerabilities: List[Vulnerability] = []
        
        # Mock vulnerability database (in real implementation, would use OSV, NVD, etc.)
        self.vuln_db = {
            "openssl": {
                "1.1.0": [
                    Vulnerability(
                        id="VULN-2023-001",
                        severity="HIGH",
                        title="OpenSSL Buffer Overflow",
                        description="Buffer overflow in OpenSSL certificate parsing",
                        affected_package="openssl",
                        affected_version="1.1.0",
                        fixed_version="1.1.1",
                        cve_id="CVE-2023-12345",
                        cvss_score=7.5
                    )
                ]
            },
            "zlib": {
                "1.2.8": [
                    Vulnerability(
                        id="VULN-2023-002", 
                        severity="MEDIUM",
                        title="zlib Compression Bomb",
                        description="Denial of service via malformed compressed data",
                        affected_package="zlib",
                        affected_version="1.2.8",
                        fixed_version="1.2.11",
                        cve_id="CVE-2023-67890",
                        cvss_score=5.3
                    )
                ]
            }
        }
        
    def scan_vcpkg_dependencies(self) -> List[Tuple[str, str]]:
        """Extract dependencies from vcpkg.json"""
        dependencies = []
        vcpkg_file = self.project_root / "vcpkg.json"
        
        if vcpkg_file.exists():
            try:
                with open(vcpkg_file) as f:
                    vcpkg_data = json.load(f)
                    
                for dep in vcpkg_data.get("dependencies", []):
                    if isinstance(dep, str):
                        dependencies.append((dep, "latest"))
                    elif isinstance(dep, dict):
                        name = dep.get("name", "")
                        version = dep.get("version", "latest")
                        dependencies.append((name, version))
                        
            except (json.JSONDecodeError, FileNotFoundError) as e:
                print(f"Warning: Could not parse vcpkg.json: {e}")
                
        return dependencies
        
    def scan_docker_dependencies(self) -> List[Tuple[str, str]]:
        """Extract dependencies from Dockerfile"""
        dependencies = []
        dockerfile = self.project_root / "Dockerfile"
        
        if dockerfile.exists():
            try:
                with open(dockerfile) as f:
                    content = f.read()
                    
                # Look for base images
                import re
                from_pattern = re.compile(r'^FROM\s+([^:\s]+)(?::([^\s]+))?', re.MULTILINE)
                
                for match in from_pattern.finditer(content):
                    image = match.group(1)
                    tag = match.group(2) or "latest"
                    dependencies.append((f"docker:{image}", tag))
                    
                # Look for apt packages
                apt_pattern = re.compile(r'apt-get install.*?([^\\]+)', re.MULTILINE | re.DOTALL)
                for match in apt_pattern.finditer(content):
                    packages = match.group(1).split()
                    for pkg in packages:
                        if pkg and not pkg.startswith('-') and pkg != '\\':
                            dependencies.append((f"apt:{pkg}", "system"))
                            
            except FileNotFoundError:
                pass
                
        return dependencies
        
    def check_vulnerability_database(self, package: str, version: str) -> List[Vulnerability]:
        """Check package against vulnerability database"""
        vulnerabilities = []
        
        # Normalize package name
        package_name = package.replace("vcpkg::", "").replace("docker:", "").replace("apt:", "")
        
        if package_name in self.vuln_db:
            package_vulns = self.vuln_db[package_name]
            
            # Check if version is vulnerable
            for vuln_version, vulns in package_vulns.items():
                if self.version_matches(version, vuln_version):
                    vulnerabilities.extend(vulns)
                    
        return vulnerabilities
        
    def version_matches(self, installed_version: str, vulnerable_version: str) -> bool:
        """Check if installed version matches vulnerable version"""
        # Simplified version matching - in real implementation would use semantic versioning
        if installed_version == "latest" or installed_version == "system":
            return True  # Assume vulnerable for safety
            
        return installed_version == vulnerable_version
        
    def scan_with_trivy(self) -> List[Vulnerability]:
        """Scan using Trivy (if available)"""
        vulnerabilities = []
        
        # Check if trivy is available
        if not shutil.which("trivy"):
            print("Trivy not found - skipping container vulnerability scan")
            return vulnerabilities
            
        try:
            # Scan Dockerfile
            dockerfile = self.project_root / "Dockerfile"
            if dockerfile.exists():
                result = subprocess.run([
                    "trivy", "config", "--format", "json", str(dockerfile)
                ], capture_output=True, text=True, timeout=60)
                
                if result.returncode == 0:
                    try:
                        trivy_data = json.loads(result.stdout)
                        
                        for result_item in trivy_data.get("Results", []):
                            for vuln in result_item.get("Vulnerabilities", []):
                                vulnerabilities.append(Vulnerability(
                                    id=vuln.get("VulnerabilityID", "UNKNOWN"),
                                    severity=vuln.get("Severity", "UNKNOWN"),
                                    title=vuln.get("Title", "No title"),
                                    description=vuln.get("Description", "No description"),
                                    affected_package=vuln.get("PkgName", "unknown"),
                                    affected_version=vuln.get("InstalledVersion", "unknown"),
                                    fixed_version=vuln.get("FixedVersion"),
                                    cve_id=vuln.get("VulnerabilityID") if vuln.get("VulnerabilityID", "").startswith("CVE") else None,
                                    cvss_score=vuln.get("CVSS", {}).get("nvd", {}).get("V3Score")
                                ))
                                
                    except json.JSONDecodeError:
                        print("Warning: Could not parse Trivy output")
                        
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as e:
            print(f"Warning: Trivy scan failed: {e}")
            
        return vulnerabilities
        
    def scan_with_osv(self, dependencies: List[Tuple[str, str]]) -> List[Vulnerability]:
        """Scan using OSV (Open Source Vulnerabilities) API"""
        vulnerabilities = []
        
        # OSV API endpoint
        osv_url = "https://api.osv.dev/v1/query"
        
        for package, version in dependencies[:5]:  # Limit requests for demo
            try:
                # Prepare query
                query = {
                    "package": {
                        "name": package,
                        "ecosystem": "vcpkg" if package.startswith("vcpkg::") else "npm"
                    }
                }
                
                if version != "latest" and version != "system":
                    query["version"] = version
                    
                # Make API request
                req_data = json.dumps(query).encode('utf-8')
                req = urllib.request.Request(
                    osv_url,
                    data=req_data,
                    headers={'Content-Type': 'application/json'}
                )
                
                with urllib.request.urlopen(req, timeout=10) as response:
                    osv_data = json.loads(response.read().decode())
                    
                    for vuln in osv_data.get("vulns", []):
                        vulnerabilities.append(Vulnerability(
                            id=vuln.get("id", "OSV-UNKNOWN"),
                            severity=vuln.get("database_specific", {}).get("severity", "UNKNOWN"),
                            title=vuln.get("summary", "No title"),
                            description=vuln.get("details", "No description")[:200] + "...",
                            affected_package=package,
                            affected_version=version,
                            cve_id=next((alias for alias in vuln.get("aliases", []) if alias.startswith("CVE")), None)
                        ))
                        
            except (urllib.error.URLError, urllib.error.HTTPError, json.JSONDecodeError) as e:
                print(f"Warning: OSV API request failed for {package}: {e}")
                continue
            except Exception as e:
                print(f"Warning: Unexpected error scanning {package}: {e}")
                continue
                
        return vulnerabilities
        
    def scan_all_dependencies(self) -> Dict[str, List[Vulnerability]]:
        """Scan all dependencies for vulnerabilities"""
        results = {
            'vcpkg': [],
            'docker': [],
            'trivy': [],
            'osv': [],
            'summary': {}
        }
        
        # Scan vcpkg dependencies
        vcpkg_deps = self.scan_vcpkg_dependencies()
        for package, version in vcpkg_deps:
            vulns = self.check_vulnerability_database(package, version)
            results['vcpkg'].extend(vulns)
            
        # Scan Docker dependencies
        docker_deps = self.scan_docker_dependencies()
        for package, version in docker_deps:
            vulns = self.check_vulnerability_database(package, version)
            results['docker'].extend(vulns)
            
        # Scan with external tools
        results['trivy'] = self.scan_with_trivy()
        
        # OSV scan (commented out to avoid API spam in demo)
        # results['osv'] = self.scan_with_osv(vcpkg_deps + docker_deps)
        
        # Generate summary
        all_vulns = (results['vcpkg'] + results['docker'] + 
                    results['trivy'] + results['osv'])
        
        severity_counts = {}
        for vuln in all_vulns:
            severity_counts[vuln.severity] = severity_counts.get(vuln.severity, 0) + 1
            
        results['summary'] = {
            'total_vulnerabilities': len(all_vulns),
            'severity_breakdown': severity_counts,
            'high_severity': len([v for v in all_vulns if v.severity == 'HIGH']),
            'critical_severity': len([v for v in all_vulns if v.severity == 'CRITICAL'])
        }
        
        return results
        
    def generate_report(self, results: Dict, output_format: str = 'text') -> str:
        """Generate vulnerability report"""
        
        if output_format == 'json':
            return json.dumps({
                'summary': results['summary'],
                'vulnerabilities': [vuln.to_dict() for vuln in 
                                  results['vcpkg'] + results['docker'] + 
                                  results['trivy'] + results['osv']]
            }, indent=2)
            
        # Text format
        report = []
        report.append("VoxelVK Vulnerability Scan Report")
        report.append("=" * 40)
        report.append("")
        
        summary = results['summary']
        report.append(f"Total Vulnerabilities: {summary['total_vulnerabilities']}")
        report.append(f"Critical Severity: {summary['critical_severity']}")
        report.append(f"High Severity: {summary['high_severity']}")
        report.append("")
        
        if summary['severity_breakdown']:
            report.append("Severity Breakdown:")
            for severity, count in summary['severity_breakdown'].items():
                report.append(f"  {severity}: {count}")
            report.append("")
            
        # Detailed vulnerabilities
        all_vulns = (results['vcpkg'] + results['docker'] + 
                    results['trivy'] + results['osv'])
        
        if all_vulns:
            report.append("Vulnerability Details:")
            report.append("-" * 25)
            
            for vuln in sorted(all_vulns, key=lambda v: v.severity, reverse=True):
                report.append(f"  {vuln.id} [{vuln.severity}]")
                report.append(f"    Package: {vuln.affected_package} ({vuln.affected_version})")
                report.append(f"    Title: {vuln.title}")
                if vuln.fixed_version:
                    report.append(f"    Fixed in: {vuln.fixed_version}")
                if vuln.cve_id:
                    report.append(f"    CVE: {vuln.cve_id}")
                report.append("")
        else:
            report.append("No vulnerabilities found!")
            
        return '\n'.join(report)

def main():
    parser = argparse.ArgumentParser(description='Scan VoxelVK for vulnerabilities')
    parser.add_argument('--project-root', default='.',
                       help='Project root directory')
    parser.add_argument('--output', '-o',
                       help='Output report file')
    parser.add_argument('--format', choices=['text', 'json'], default='text',
                       help='Output format')
    parser.add_argument('--fail-on-high', action='store_true',
                       help='Exit with error if high/critical vulnerabilities found')
    parser.add_argument('--skip-external', action='store_true',
                       help='Skip external vulnerability scans (Trivy, OSV)')
    
    args = parser.parse_args()
    
    scanner = VulnerabilityScanner(args.project_root)
    results = scanner.scan_all_dependencies()
    report = scanner.generate_report(results, args.format)
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"Vulnerability report saved to: {args.output}")
    else:
        print(report)
        
    # Exit with error if high/critical vulnerabilities found
    if args.fail_on_high:
        high_count = results['summary']['high_severity']
        critical_count = results['summary']['critical_severity']
        
        if high_count > 0 or critical_count > 0:
            print(f"\nERROR: Found {critical_count} critical and {high_count} high severity vulnerabilities")
            sys.exit(1)
            
    print(f"\nScan complete: {results['summary']['total_vulnerabilities']} vulnerabilities found")

if __name__ == '__main__':
    main()