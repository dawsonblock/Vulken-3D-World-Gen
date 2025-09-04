#!/usr/bin/env python3
"""
Vulken-3D Duplicate Detection and Purge Script
==================================================

This script removes exact duplicate files and unwanted artifacts from the repository
while preserving the canonical source code structure.

Phase 1 of the Production Upgrade Plan.
"""

import os
import sys
import hashlib
import json
import shutil
from pathlib import Path
from typing import Dict, List, Set, Tuple
from collections import defaultdict
import argparse


class DedupePurger:
    def __init__(self, repo_root: str, dry_run: bool = False):
        self.repo_root = Path(repo_root)
        self.dry_run = dry_run
        self.report = {
            "removed_files": [],
            "removed_directories": [],
            "duplicates_resolved": [],
            "bytes_saved": 0,
            "protected_files": [],
            "scan_summary": {}
        }
        
        # Define patterns for removal (KEEP vs REMOVE ruleset)
        self.PROTECTED_PATHS = {
            "src/", "include/", "apps/", "cmake/", "shaders/", "config/", 
            "scripts/", "tests/", "docs/", ".github/", "external/", "tools/"
        }
        
        self.REMOVE_PATTERNS = {
            # Archive files
            "*.zip", "*.7z", "*.rar", "*.tar.Z", "*.tar.gz", "*.tar.bz2",
            # Editor/system artifacts  
            "*.DS_Store", "Thumbs.db", "*.user", "*.VC.db", "imgui.ini", 
            "screenshot.png", "*.log",
            # Build artifacts
            "build*/", "out/", "dist/", 
            # Cache/temp directories
            ".idea/", ".vscode/*.cache", "__pycache__/", "*.pyc", "cache/",
            # Backup directories
            "old/", "backup/", "tmp/"
        }
        
        self.DUPLICATE_DIRECTORIES = {
            "Vulken-3D-World-Gen-main-2/",  # Primary duplicate to remove
            "__MACOSX/"  # macOS metadata
        }

    def calculate_file_hash(self, filepath: Path) -> str:
        """Calculate SHA256 hash of file contents."""
        sha256_hash = hashlib.sha256()
        try:
            with open(filepath, "rb") as f:
                for byte_block in iter(lambda: f.read(4096), b""):
                    sha256_hash.update(byte_block)
            return sha256_hash.hexdigest()
        except (IOError, OSError) as e:
            print(f"Warning: Could not hash {filepath}: {e}")
            return ""

    def is_protected_path(self, path: Path) -> bool:
        """Check if path is in protected directories."""
        path_str = str(path.relative_to(self.repo_root))
        return any(path_str.startswith(protected) for protected in self.PROTECTED_PATHS)

    def should_remove_by_pattern(self, path: Path) -> bool:
        """Check if file/directory matches removal patterns."""
        path_str = str(path.relative_to(self.repo_root))
        
        # Check exact directory matches
        for dup_dir in self.DUPLICATE_DIRECTORIES:
            if path_str.startswith(dup_dir):
                return True
        
        # Check file patterns (simplified glob matching)
        name = path.name
        for pattern in self.REMOVE_PATTERNS:
            if pattern.startswith("*.") and name.endswith(pattern[1:]):
                return True
            elif pattern.endswith("/") and path.is_dir() and path_str.endswith(pattern[:-1]):
                return True
                
        return False

    def find_duplicates(self) -> Dict[str, List[Path]]:
        """Find files with identical content (by hash)."""
        hash_to_paths = defaultdict(list)
        
        print("🔍 Scanning for duplicate files...")
        for filepath in self.repo_root.rglob("*"):
            if filepath.is_file() and not self.should_remove_by_pattern(filepath):
                file_hash = self.calculate_file_hash(filepath)
                if file_hash:
                    hash_to_paths[file_hash].append(filepath)
        
        # Filter to only actual duplicates (more than one file per hash)
        duplicates = {h: paths for h, paths in hash_to_paths.items() if len(paths) > 1}
        
        print(f"📊 Found {len(duplicates)} sets of duplicate files")
        return duplicates

    def choose_canonical_path(self, duplicate_paths: List[Path]) -> Path:
        """Choose the canonical path to keep (shortest path wins)."""
        # Sort by path length, then alphabetically for determinism
        sorted_paths = sorted(duplicate_paths, key=lambda p: (len(str(p)), str(p)))
        canonical = sorted_paths[0]
        
        # Prefer non-duplicate directories if available
        for path in sorted_paths:
            path_str = str(path.relative_to(self.repo_root))
            if not any(path_str.startswith(dup_dir) for dup_dir in self.DUPLICATE_DIRECTORIES):
                return path
                
        return canonical

    def safe_remove(self, path: Path) -> bool:
        """Safely remove file/directory with error handling."""
        try:
            if self.dry_run:
                print(f"  [DRY-RUN] Would remove: {path}")
                return True
            
            if path.is_file():
                size = path.stat().st_size
                path.unlink()
                self.report["bytes_saved"] += size
                self.report["removed_files"].append(str(path.relative_to(self.repo_root)))
                print(f"  ✅ Removed file: {path.relative_to(self.repo_root)}")
            elif path.is_dir():
                size = sum(f.stat().st_size for f in path.rglob("*") if f.is_file())
                shutil.rmtree(path)
                self.report["bytes_saved"] += size
                self.report["removed_directories"].append(str(path.relative_to(self.repo_root)))
                print(f"  ✅ Removed directory: {path.relative_to(self.repo_root)}")
            
            return True
        except (OSError, IOError) as e:
            print(f"  ❌ Failed to remove {path}: {e}")
            return False

    def deduplicate_files(self) -> None:
        """Remove duplicate files, keeping canonical versions."""
        duplicates = self.find_duplicates()
        
        for file_hash, duplicate_paths in duplicates.items():
            canonical = self.choose_canonical_path(duplicate_paths)
            
            print(f"\n📂 Duplicate set (hash: {file_hash[:8]}...):")
            print(f"  📌 Canonical: {canonical.relative_to(self.repo_root)}")
            
            duplicates_to_remove = [p for p in duplicate_paths if p != canonical]
            
            for dup_path in duplicates_to_remove:
                if self.is_protected_path(dup_path):
                    print(f"  ⚠️  Protected (skipping): {dup_path.relative_to(self.repo_root)}")
                    self.report["protected_files"].append(str(dup_path.relative_to(self.repo_root)))
                elif self.safe_remove(dup_path):
                    self.report["duplicates_resolved"].append({
                        "removed": str(dup_path.relative_to(self.repo_root)),
                        "canonical": str(canonical.relative_to(self.repo_root)),
                        "hash": file_hash[:16]
                    })

    def purge_artifacts(self) -> None:
        """Remove unwanted artifacts and temporary files."""
        print("\n🧹 Purging unwanted artifacts...")
        
        # Remove duplicate directories entirely
        for dup_dir in self.DUPLICATE_DIRECTORIES:
            dup_path = self.repo_root / dup_dir
            if dup_path.exists():
                print(f"📁 Removing duplicate directory: {dup_dir}")
                self.safe_remove(dup_path)
        
        # Remove files by patterns
        for pattern in self.REMOVE_PATTERNS:
            if pattern.startswith("*."):  # File extension patterns
                ext = pattern[1:]
                for filepath in self.repo_root.rglob(f"*{ext}"):
                    if filepath.is_file() and not self.is_protected_path(filepath):
                        self.safe_remove(filepath)

    def update_gitignore(self) -> None:
        """Update .gitignore with comprehensive patterns."""
        gitignore_path = self.repo_root / ".gitignore"
        
        additional_patterns = [
            "# Build artifacts",
            "build*/", "out/", "dist/", "*.log",
            "",
            "# Editor artifacts", 
            "*.DS_Store", "Thumbs.db", "*.user", "*.VC.db",
            ".idea/", ".vscode/.cache/",
            "",
            "# Cache and temporary files",
            "__pycache__/", "*.pyc", "cache/", "tmp/",
            "imgui.ini", "screenshot.png",
            "",
            "# Archives (unless specifically tracked)",
            "*.zip", "*.7z", "*.rar", "*.tar.Z"
        ]
        
        if not self.dry_run:
            try:
                with open(gitignore_path, "a") as f:
                    f.write("\n# Added by dedupe_and_purge.py\n")
                    f.write("\n".join(additional_patterns))
                    f.write("\n")
                print(f"  ✅ Updated {gitignore_path}")
            except IOError as e:
                print(f"  ❌ Failed to update .gitignore: {e}")

    def generate_report(self, output_path: str) -> None:
        """Generate detailed JSON report of operations."""
        self.report["scan_summary"] = {
            "total_files_removed": len(self.report["removed_files"]),
            "total_directories_removed": len(self.report["removed_directories"]), 
            "duplicates_resolved": len(self.report["duplicates_resolved"]),
            "bytes_saved": self.report["bytes_saved"],
            "bytes_saved_mb": round(self.report["bytes_saved"] / (1024 * 1024), 2),
            "protected_files_found": len(self.report["protected_files"])
        }
        
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        with open(output_path, "w") as f:
            json.dump(self.report, f, indent=2)
        
        print(f"\n📊 Report saved to: {output_path}")
        print(f"💾 Space saved: {self.report['scan_summary']['bytes_saved_mb']:.2f} MB")

    def run(self, report_path: str) -> None:
        """Execute the complete deduplication and purge process."""
        print("🚀 Starting Vulken-3D Deduplication and Purge")
        print(f"📁 Repository root: {self.repo_root}")
        print(f"🔧 Mode: {'DRY-RUN' if self.dry_run else 'LIVE'}")
        
        # Phase 1: Remove duplicate files
        self.deduplicate_files()
        
        # Phase 2: Purge unwanted artifacts  
        self.purge_artifacts()
        
        # Phase 3: Update .gitignore
        self.update_gitignore()
        
        # Phase 4: Generate report
        self.generate_report(report_path)
        
        print("\n✅ Deduplication and purge complete!")


def main():
    parser = argparse.ArgumentParser(description="Vulken-3D duplicate removal and artifact purging")
    parser.add_argument("--repo-root", default=".", help="Repository root directory")
    parser.add_argument("--report", default="reports/cleanup/dedupe_report.json", 
                       help="Output report path")
    parser.add_argument("--dry-run", action="store_true", 
                       help="Show what would be removed without actually doing it")
    
    args = parser.parse_args()
    
    # Resolve repository root
    repo_root = os.path.abspath(args.repo_root)
    if not os.path.isdir(repo_root):
        print(f"❌ Error: Repository root not found: {repo_root}")
        sys.exit(1)
    
    # Create purger and run
    purger = DedupePurger(repo_root, dry_run=args.dry_run)
    purger.run(args.report)
    
    if args.dry_run:
        print("\n🔄 Run without --dry-run to execute the changes.")


if __name__ == "__main__":
    main()