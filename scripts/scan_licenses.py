#!/usr/bin/env python3
"""
VoxelVK License Scanner

Scans the codebase for license compliance issues.
Checks source files for proper license headers.
"""

import os
import re
import sys
import argparse
from pathlib import Path
from typing import List, Dict, Set, Tuple
from dataclasses import dataclass

@dataclass
class LicenseIssue:
    file_path: str
    issue_type: str
    description: str
    line_number: int = 0

class LicenseScanner:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.issues: List[LicenseIssue] = []
        
        # Expected license header patterns
        self.license_patterns = [
            re.compile(r'Copyright.*VoxelVK', re.IGNORECASE),
            re.compile(r'MIT License', re.IGNORECASE),
            re.compile(r'Licensed under.*MIT', re.IGNORECASE),
        ]
        
        # File extensions to scan
        self.source_extensions = {'.cpp', '.hpp', '.c', '.h', '.cc', '.cxx', '.hxx'}
        self.script_extensions = {'.py', '.sh', '.js', '.ts'}
        self.all_extensions = self.source_extensions | self.script_extensions
        
        # Files/directories to skip
        self.skip_patterns = [
            re.compile(r'build.*'),
            re.compile(r'\.cache'),
            re.compile(r'vcpkg_installed'),
            re.compile(r'\.git'),
            re.compile(r'third_party'),
            re.compile(r'external'),
            re.compile(r'\.vs'),
            re.compile(r'\.vscode'),
        ]
        
    def should_skip_file(self, filepath: Path) -> bool:
        """Check if file should be skipped"""
        path_str = str(filepath)
        
        for pattern in self.skip_patterns:
            if pattern.search(path_str):
                return True
                
        return False
        
    def scan_file_license(self, filepath: Path) -> List[LicenseIssue]:
        """Scan a single file for license compliance"""
        issues = []
        
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
            # Check for license header in first 20 lines
            header_section = '\n'.join(lines[:20])
            
            has_copyright = any(pattern.search(header_section) for pattern in self.license_patterns)
            
            if not has_copyright:
                issues.append(LicenseIssue(
                    file_path=str(filepath.relative_to(self.project_root)),
                    issue_type="MISSING_LICENSE_HEADER",
                    description="No license header found in file"
                ))
                
            # Check for problematic license terms
            problematic_terms = [
                (r'proprietary', "Proprietary license reference"),
                (r'all rights reserved', "All rights reserved clause"),
                (r'confidential', "Confidential marking"),
            ]
            
            for pattern, description in problematic_terms:
                if re.search(pattern, content, re.IGNORECASE):
                    for i, line in enumerate(lines):
                        if re.search(pattern, line, re.IGNORECASE):
                            issues.append(LicenseIssue(
                                file_path=str(filepath.relative_to(self.project_root)),
                                issue_type="PROBLEMATIC_LICENSE_TERM",
                                description=description,
                                line_number=i + 1
                            ))
                            break
                            
        except Exception as e:
            issues.append(LicenseIssue(
                file_path=str(filepath.relative_to(self.project_root)),
                issue_type="SCAN_ERROR",
                description=f"Error scanning file: {e}"
            ))
            
        return issues
        
    def scan_dependency_licenses(self) -> List[LicenseIssue]:
        """Scan dependency licenses for compatibility"""
        issues = []
        
        # Check vcpkg.json
        vcpkg_file = self.project_root / "vcpkg.json"
        if vcpkg_file.exists():
            try:
                import json
                with open(vcpkg_file) as f:
                    vcpkg_data = json.load(f)
                    
                # Known problematic dependencies (example)
                problematic_deps = {
                    'gpl-licensed-lib': 'GPL license incompatible with MIT',
                    'proprietary-sdk': 'Proprietary license'
                }
                
                for dep in vcpkg_data.get("dependencies", []):
                    dep_name = dep if isinstance(dep, str) else dep.get("name", "")
                    
                    if dep_name in problematic_deps:
                        issues.append(LicenseIssue(
                            file_path="vcpkg.json",
                            issue_type="INCOMPATIBLE_DEPENDENCY",
                            description=f"{dep_name}: {problematic_deps[dep_name]}"
                        ))
                        
            except Exception as e:
                issues.append(LicenseIssue(
                    file_path="vcpkg.json",
                    issue_type="SCAN_ERROR",
                    description=f"Error scanning dependencies: {e}"
                ))
                
        return issues
        
    def scan_all_files(self) -> Dict[str, List[LicenseIssue]]:
        """Scan all source files in the project"""
        results = {
            'source_files': [],
            'dependencies': [],
            'summary': {}
        }
        
        # Scan source files
        for ext in self.all_extensions:
            pattern = f"**/*{ext}"
            for filepath in self.project_root.glob(pattern):
                if self.should_skip_file(filepath):
                    continue
                    
                file_issues = self.scan_file_license(filepath)
                if file_issues:
                    results['source_files'].extend(file_issues)
                    
        # Scan dependencies
        dep_issues = self.scan_dependency_licenses()
        results['dependencies'].extend(dep_issues)
        
        # Generate summary
        all_issues = results['source_files'] + results['dependencies']
        issue_counts = {}
        for issue in all_issues:
            issue_counts[issue.issue_type] = issue_counts.get(issue.issue_type, 0) + 1
            
        results['summary'] = {
            'total_issues': len(all_issues),
            'source_file_issues': len(results['source_files']),
            'dependency_issues': len(results['dependencies']),
            'issue_types': issue_counts
        }
        
        return results
        
    def generate_report(self, results: Dict, output_format: str = 'text') -> str:
        """Generate license compliance report"""
        
        if output_format == 'json':
            import json
            return json.dumps({
                'summary': results['summary'],
                'issues': [
                    {
                        'file': issue.file_path,
                        'type': issue.issue_type,
                        'description': issue.description,
                        'line': issue.line_number
                    }
                    for issue in results['source_files'] + results['dependencies']
                ]
            }, indent=2)
            
        # Text format
        report = []
        report.append("VoxelVK License Compliance Report")
        report.append("=" * 40)
        report.append("")
        
        summary = results['summary']
        report.append(f"Total Issues: {summary['total_issues']}")
        report.append(f"Source File Issues: {summary['source_file_issues']}")
        report.append(f"Dependency Issues: {summary['dependency_issues']}")
        report.append("")
        
        if summary['issue_types']:
            report.append("Issue Types:")
            for issue_type, count in summary['issue_types'].items():
                report.append(f"  {issue_type}: {count}")
            report.append("")
            
        # Detailed issues
        if results['source_files']:
            report.append("Source File Issues:")
            report.append("-" * 20)
            for issue in results['source_files']:
                line_info = f" (line {issue.line_number})" if issue.line_number > 0 else ""
                report.append(f"  {issue.file_path}{line_info}")
                report.append(f"    {issue.issue_type}: {issue.description}")
            report.append("")
            
        if results['dependencies']:
            report.append("Dependency Issues:")
            report.append("-" * 18)
            for issue in results['dependencies']:
                report.append(f"  {issue.file_path}")
                report.append(f"    {issue.issue_type}: {issue.description}")
            report.append("")
            
        # Recommendations
        report.append("Recommendations:")
        report.append("-" * 15)
        
        if summary['source_file_issues'] > 0:
            report.append("  - Add MIT license headers to source files")
            report.append("  - Use consistent copyright notice format")
            
        if summary['dependency_issues'] > 0:
            report.append("  - Review dependency licenses for compatibility")
            report.append("  - Consider alternatives for incompatible dependencies")
            
        if summary['total_issues'] == 0:
            report.append("  - No issues found! License compliance looks good.")
            
        return '\n'.join(report)

def main():
    parser = argparse.ArgumentParser(description='Scan VoxelVK for license compliance')
    parser.add_argument('--project-root', default='.',
                       help='Project root directory')
    parser.add_argument('--output', '-o',
                       help='Output report file')
    parser.add_argument('--format', choices=['text', 'json'], default='text',
                       help='Output format')
    parser.add_argument('--fail-on-issues', action='store_true',
                       help='Exit with error code if issues found')
    
    args = parser.parse_args()
    
    scanner = LicenseScanner(args.project_root)
    results = scanner.scan_all_files()
    report = scanner.generate_report(results, args.format)
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"License scan report saved to: {args.output}")
    else:
        print(report)
        
    # Exit with error if issues found and --fail-on-issues is set
    if args.fail_on_issues and results['summary']['total_issues'] > 0:
        sys.exit(1)
        
    print(f"\nScan complete: {results['summary']['total_issues']} issues found")

if __name__ == '__main__':
    main()