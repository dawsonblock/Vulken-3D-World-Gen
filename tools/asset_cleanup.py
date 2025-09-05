#!/usr/bin/env python3
"""
VoxelVK Asset Cleanup and External Asset Management
Identifies duplicates, large files, and manages external asset repository.
"""

import os
import sys
import json
import hashlib
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Set
import argparse

class AssetCleanup:
    def __init__(self, assets_dir: str = "assets", external_repo: str = "voxelvk-assets"):
        self.assets_dir = Path(assets_dir)
        self.external_repo = external_repo
        self.duplicates = []
        self.large_files = []
        self.asset_manifest = {
            "version": "1.0",
            "local_assets": {},
            "external_assets": {},
            "duplicates_removed": [],
            "large_files_moved": []
        }
        
    def find_duplicates(self) -> List[Tuple[str, List[str]]]:
        """Find duplicate files by content hash"""
        file_hashes = {}
        duplicates = []
        
        for file_path in self.assets_dir.rglob("*"):
            if file_path.is_file():
                try:
                    with open(file_path, 'rb') as f:
                        file_hash = hashlib.md5(f.read()).hexdigest()
                    
                    if file_hash in file_hashes:
                        file_hashes[file_hash].append(str(file_path))
                    else:
                        file_hashes[file_hash] = [str(file_path)]
                except Exception as e:
                    print(f"Error reading {file_path}: {e}")
        
        # Find groups of duplicates
        for file_hash, file_list in file_hashes.items():
            if len(file_list) > 1:
                duplicates.append((file_hash, file_list))
        
        return duplicates
    
    def find_large_files(self, threshold_mb: float = 1.0) -> List[Tuple[str, int]]:
        """Find files larger than threshold"""
        large_files = []
        threshold_bytes = threshold_mb * 1024 * 1024
        
        for file_path in self.assets_dir.rglob("*"):
            if file_path.is_file():
                file_size = file_path.stat().st_size
                if file_size > threshold_bytes:
                    large_files.append((str(file_path), file_size))
        
        return sorted(large_files, key=lambda x: x[1], reverse=True)
    
    def create_external_asset_script(self) -> None:
        """Create script to fetch external assets"""
        script_content = '''#!/bin/bash
# VoxelVK External Asset Fetcher
# This script fetches large assets from the external voxelvk-assets repository

set -e

EXTERNAL_REPO="https://github.com/voxelvk/voxelvk-assets.git"
ASSETS_DIR="assets"
EXTERNAL_DIR="voxelvk-assets"

echo "VoxelVK External Asset Fetcher"
echo "=============================="

# Check if git-lfs is available
if ! command -v git-lfs &> /dev/null; then
    echo "Warning: git-lfs not found. Large files may not download properly."
    echo "Install git-lfs: https://git-lfs.github.io/"
fi

# Clone or update external assets
if [ -d "$EXTERNAL_DIR" ]; then
    echo "Updating external assets..."
    cd "$EXTERNAL_DIR"
    git pull
    cd ..
else
    echo "Cloning external assets..."
    git clone "$EXTERNAL_REPO" "$EXTERNAL_DIR"
fi

# Copy assets to local directory
echo "Copying assets..."
cp -r "$EXTERNAL_DIR"/* "$ASSETS_DIR/"

echo "External assets fetched successfully!"
echo "You can now run VoxelVK with full asset support."
'''
        
        script_path = Path("scripts/fetch_external_assets.sh")
        script_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(script_path, 'w') as f:
            f.write(script_content)
        
        os.chmod(script_path, 0o755)
        print(f"Created external asset fetcher: {script_path}")
    
    def create_asset_placeholders(self) -> None:
        """Create placeholder files for external assets"""
        placeholders = {
            "textures/hd/": "High-resolution textures (moved to external repo)",
            "meshes/hd/": "High-resolution meshes (moved to external repo)",
            "sounds/": "Audio files (moved to external repo)",
            "models/": "3D models (moved to external repo)",
            "videos/": "Video assets (moved to external repo)"
        }
        
        for placeholder_dir, description in placeholders.items():
            placeholder_path = self.assets_dir / placeholder_dir
            placeholder_path.mkdir(parents=True, exist_ok=True)
            
            readme_path = placeholder_path / "README.md"
            with open(readme_path, 'w') as f:
                f.write(f"# {placeholder_dir}\n\n")
                f.write(f"{description}\n\n")
                f.write("To fetch these assets, run:\n")
                f.write("```bash\n")
                f.write("./scripts/fetch_external_assets.sh\n")
                f.write("```\n")
        
        print("Created asset placeholders for external repository")
    
    def remove_duplicates(self, dry_run: bool = True) -> None:
        """Remove duplicate files, keeping the first occurrence"""
        duplicates = self.find_duplicates()
        
        if not duplicates:
            print("No duplicate files found.")
            return
        
        print(f"Found {len(duplicates)} groups of duplicate files:")
        
        for file_hash, file_list in duplicates:
            print(f"\nDuplicate group (hash: {file_hash[:8]}...):")
            for i, file_path in enumerate(file_list):
                status = "KEEP" if i == 0 else "REMOVE"
                print(f"  {status}: {file_path}")
            
            if not dry_run:
                # Remove all but the first file
                for file_path in file_list[1:]:
                    try:
                        os.remove(file_path)
                        self.asset_manifest["duplicates_removed"].append(file_path)
                        print(f"  Removed: {file_path}")
                    except Exception as e:
                        print(f"  Error removing {file_path}: {e}")
    
    def move_large_files(self, threshold_mb: float = 1.0, dry_run: bool = True) -> None:
        """Move large files to external repository (simulate)"""
        large_files = self.find_large_files(threshold_mb)
        
        if not large_files:
            print(f"No files larger than {threshold_mb}MB found.")
            return
        
        print(f"Found {len(large_files)} files larger than {threshold_mb}MB:")
        
        for file_path, file_size in large_files:
            size_mb = file_size / (1024 * 1024)
            print(f"  {file_path} ({size_mb:.2f}MB)")
            
            if not dry_run:
                # In a real implementation, this would move files to external repo
                # For now, we'll just create a placeholder
                placeholder_path = Path(file_path + ".placeholder")
                with open(placeholder_path, 'w') as f:
                    f.write(f"# Placeholder for {file_path}\n")
                    f.write(f"# Original size: {size_mb:.2f}MB\n")
                    f.write(f"# Moved to external repository: {self.external_repo}\n")
                    f.write(f"# Fetch with: ./scripts/fetch_external_assets.sh\n")
                
                self.asset_manifest["large_files_moved"].append({
                    "original_path": file_path,
                    "size_mb": size_mb,
                    "placeholder_path": str(placeholder_path)
                })
    
    def generate_asset_manifest(self) -> None:
        """Generate comprehensive asset manifest"""
        # Scan local assets
        for file_path in self.assets_dir.rglob("*"):
            if file_path.is_file():
                rel_path = file_path.relative_to(self.assets_dir)
                file_size = file_path.stat().st_size
                
                self.asset_manifest["local_assets"][str(rel_path)] = {
                    "size_bytes": file_size,
                    "size_mb": file_size / (1024 * 1024),
                    "type": file_path.suffix,
                    "path": str(file_path)
                }
        
        # Write manifest
        manifest_path = self.assets_dir / "asset_manifest.json"
        with open(manifest_path, 'w') as f:
            json.dump(self.asset_manifest, f, indent=2)
        
        print(f"Asset manifest written to: {manifest_path}")
    
    def run_cleanup(self, dry_run: bool = True) -> None:
        """Run complete asset cleanup"""
        print("VoxelVK Asset Cleanup")
        print("=" * 20)
        print(f"Assets directory: {self.assets_dir}")
        print(f"External repository: {self.external_repo}")
        print(f"Mode: {'DRY RUN' if dry_run else 'LIVE'}")
        print()
        
        # Find and report duplicates
        duplicates = self.find_duplicates()
        if duplicates:
            print(f"Found {len(duplicates)} duplicate file groups")
            self.remove_duplicates(dry_run)
        else:
            print("No duplicate files found")
        
        print()
        
        # Find and report large files
        large_files = self.find_large_files()
        if large_files:
            print(f"Found {len(large_files)} large files")
            self.move_large_files(dry_run=dry_run)
        else:
            print("No large files found")
        
        print()
        
        # Create external asset infrastructure
        self.create_external_asset_script()
        self.create_asset_placeholders()
        
        # Generate manifest
        self.generate_asset_manifest()
        
        print("\nAsset cleanup complete!")

def main():
    parser = argparse.ArgumentParser(description='VoxelVK Asset Cleanup Tool')
    parser.add_argument('--assets-dir', default='assets',
                        help='Assets directory to clean up')
    parser.add_argument('--external-repo', default='voxelvk-assets',
                        help='External asset repository name')
    parser.add_argument('--threshold-mb', type=float, default=1.0,
                        help='Size threshold in MB for moving files to external repo')
    parser.add_argument('--dry-run', action='store_true', default=True,
                        help='Perform dry run without making changes')
    parser.add_argument('--live', action='store_true',
                        help='Perform live cleanup (overrides --dry-run)')
    
    args = parser.parse_args()
    
    if args.live:
        args.dry_run = False
    
    cleanup = AssetCleanup(args.assets_dir, args.external_repo)
    cleanup.run_cleanup(dry_run=args.dry_run)

if __name__ == '__main__':
    main()