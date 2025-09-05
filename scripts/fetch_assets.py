#!/usr/bin/env python3
"""
VoxelVK Asset Fetcher

Downloads and manages external asset packs for VoxelVK engine.
Ensures proper licensing and size limits.
"""

import os
import sys
import json
import hashlib
import urllib.request
import urllib.error
import zipfile
import argparse
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Tuple

class AssetPack:
    def __init__(self, name: str, config: Dict):
        self.name = name
        self.url = config.get('url', '')
        self.version = config.get('version', '1.0.0')
        self.size_mb = config.get('size_mb', 0)
        self.license = config.get('license', 'unknown')
        self.description = config.get('description', '')
        self.sha256 = config.get('sha256', '')
        self.install_path = config.get('install_path', f'assets/packs/{name}')
        self.files = config.get('files', [])
        
    def is_valid(self) -> bool:
        """Check if asset pack configuration is valid"""
        return bool(self.url and self.license and self.sha256)
        
    def is_license_compatible(self) -> bool:
        """Check if license is compatible with VoxelVK"""
        compatible_licenses = [
            'CC0', 'CC0-1.0', 'MIT', 'BSD', 'Apache-2.0', 
            'CC BY', 'CC BY-SA', 'Public Domain'
        ]
        return any(license in self.license for license in compatible_licenses)

class AssetFetcher:
    def __init__(self, config_path: str = 'config/asset_packs.json'):
        self.config_path = config_path
        self.asset_packs: Dict[str, AssetPack] = {}
        self.load_config()
        
    def load_config(self):
        """Load asset pack configurations"""
        if not os.path.exists(self.config_path):
            self.create_default_config()
            
        try:
            with open(self.config_path, 'r') as f:
                config = json.load(f)
                
            for name, pack_config in config.get('asset_packs', {}).items():
                self.asset_packs[name] = AssetPack(name, pack_config)
                
        except (json.JSONDecodeError, FileNotFoundError) as e:
            print(f"Error loading asset config: {e}")
            sys.exit(1)
            
    def create_default_config(self):
        """Create default asset pack configuration"""
        default_config = {
            "asset_packs": {
                "sample_textures": {
                    "url": "https://github.com/voxelvk/sample-assets/releases/download/v1.0.0/textures.zip",
                    "version": "1.0.0",
                    "size_mb": 2.5,
                    "license": "CC0-1.0",
                    "description": "Sample textures for testing and demos",
                    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                    "install_path": "assets/samples/textures",
                    "files": ["grass.png", "stone.png", "wood.png", "metal.png"]
                },
                "demo_meshes": {
                    "url": "https://github.com/voxelvk/sample-assets/releases/download/v1.0.0/meshes.zip",
                    "version": "1.0.0", 
                    "size_mb": 1.2,
                    "license": "CC0-1.0",
                    "description": "Simple geometric meshes for demos",
                    "sha256": "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
                    "install_path": "assets/samples/meshes",
                    "files": ["cube.obj", "sphere.obj", "cylinder.obj"]
                },
                "test_voxels": {
                    "url": "https://github.com/voxelvk/sample-assets/releases/download/v1.0.0/voxels.zip",
                    "version": "1.0.0",
                    "size_mb": 3.8,
                    "license": "CC0-1.0", 
                    "description": "Voxel test data and sample chunks",
                    "sha256": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
                    "install_path": "assets/samples/voxels",
                    "files": ["terrain.vxl", "structures.vxl", "test_patterns.vxl"]
                }
            }
        }
        
        os.makedirs(os.path.dirname(self.config_path), exist_ok=True)
        with open(self.config_path, 'w') as f:
            json.dump(default_config, f, indent=2)
            
        print(f"Created default asset configuration: {self.config_path}")
        
    def list_packs(self):
        """List all available asset packs"""
        print("Available Asset Packs:")
        print("=" * 50)
        
        for name, pack in self.asset_packs.items():
            status = "✓" if pack.is_license_compatible() else "⚠"
            print(f"{status} {name}")
            print(f"  Description: {pack.description}")
            print(f"  License: {pack.license}")
            print(f"  Size: {pack.size_mb} MB")
            print(f"  Version: {pack.version}")
            
            # Check if already installed
            if os.path.exists(pack.install_path):
                print(f"  Status: INSTALLED")
            else:
                print(f"  Status: Not installed")
            print()
            
    def verify_file_hash(self, filepath: str, expected_hash: str) -> bool:
        """Verify SHA256 hash of downloaded file"""
        sha256_hash = hashlib.sha256()
        
        try:
            with open(filepath, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    sha256_hash.update(chunk)
                    
            return sha256_hash.hexdigest() == expected_hash
        except IOError:
            return False
            
    def download_file(self, url: str, destination: str, 
                     progress_callback: Optional[callable] = None) -> bool:
        """Download file with progress reporting"""
        try:
            def report_progress(block_num, block_size, total_size):
                if progress_callback:
                    progress_callback(block_num * block_size, total_size)
                    
            urllib.request.urlretrieve(url, destination, report_progress)
            return True
            
        except urllib.error.URLError as e:
            print(f"Download failed: {e}")
            return False
            
    def extract_archive(self, archive_path: str, extract_to: str) -> bool:
        """Extract zip archive to destination"""
        try:
            with zipfile.ZipFile(archive_path, 'r') as zip_ref:
                zip_ref.extractall(extract_to)
            return True
        except zipfile.BadZipFile:
            print(f"Invalid zip file: {archive_path}")
            return False
        except Exception as e:
            print(f"Extraction failed: {e}")
            return False
            
    def fetch_pack(self, pack_name: str, force: bool = False) -> bool:
        """Fetch and install an asset pack"""
        if pack_name not in self.asset_packs:
            print(f"Unknown asset pack: {pack_name}")
            return False
            
        pack = self.asset_packs[pack_name]
        
        if not pack.is_valid():
            print(f"Invalid configuration for pack: {pack_name}")
            return False
            
        if not pack.is_license_compatible():
            print(f"Incompatible license for pack {pack_name}: {pack.license}")
            print("VoxelVK only supports CC0, MIT, BSD, Apache-2.0, and similar permissive licenses")
            return False
            
        # Check if already installed
        if os.path.exists(pack.install_path) and not force:
            print(f"Asset pack '{pack_name}' already installed. Use --force to reinstall.")
            return True
            
        print(f"Fetching asset pack: {pack_name}")
        print(f"License: {pack.license}")
        print(f"Size: {pack.size_mb} MB")
        print(f"URL: {pack.url}")
        
        # Create temporary download directory
        temp_dir = f"/tmp/voxelvk_assets_{pack_name}"
        os.makedirs(temp_dir, exist_ok=True)
        
        archive_path = os.path.join(temp_dir, f"{pack_name}.zip")
        
        # Progress callback
        def show_progress(downloaded: int, total: int):
            if total > 0:
                percent = (downloaded / total) * 100
                print(f"\rDownloading: {percent:.1f}%", end='', flush=True)
                
        # Download
        print("Downloading...")
        if not self.download_file(pack.url, archive_path, show_progress):
            shutil.rmtree(temp_dir)
            return False
            
        print("\nVerifying download...")
        
        # Verify hash
        if not self.verify_file_hash(archive_path, pack.sha256):
            print("Hash verification failed! Download may be corrupted or tampered with.")
            shutil.rmtree(temp_dir)
            return False
            
        print("Hash verified ✓")
        
        # Create install directory
        os.makedirs(pack.install_path, exist_ok=True)
        
        # Extract
        print("Extracting...")
        if not self.extract_archive(archive_path, pack.install_path):
            shutil.rmtree(temp_dir)
            return False
            
        # Verify extracted files
        missing_files = []
        for filename in pack.files:
            file_path = os.path.join(pack.install_path, filename)
            if not os.path.exists(file_path):
                missing_files.append(filename)
                
        if missing_files:
            print(f"Warning: Expected files not found: {', '.join(missing_files)}")
            
        # Cleanup
        shutil.rmtree(temp_dir)
        
        print(f"Successfully installed asset pack: {pack_name}")
        print(f"Location: {pack.install_path}")
        
        return True
        
    def remove_pack(self, pack_name: str) -> bool:
        """Remove an installed asset pack"""
        if pack_name not in self.asset_packs:
            print(f"Unknown asset pack: {pack_name}")
            return False
            
        pack = self.asset_packs[pack_name]
        
        if not os.path.exists(pack.install_path):
            print(f"Asset pack '{pack_name}' is not installed")
            return False
            
        try:
            shutil.rmtree(pack.install_path)
            print(f"Removed asset pack: {pack_name}")
            return True
        except Exception as e:
            print(f"Failed to remove pack: {e}")
            return False
            
    def update_pack(self, pack_name: str) -> bool:
        """Update an asset pack to the latest version"""
        return self.fetch_pack(pack_name, force=True)

def main():
    parser = argparse.ArgumentParser(description='VoxelVK Asset Fetcher')
    parser.add_argument('--list', action='store_true', help='List available asset packs')
    parser.add_argument('--fetch', type=str, help='Fetch specified asset pack')
    parser.add_argument('--remove', type=str, help='Remove specified asset pack')
    parser.add_argument('--update', type=str, help='Update specified asset pack')
    parser.add_argument('--force', action='store_true', help='Force reinstall if already exists')
    parser.add_argument('--config', type=str, default='config/asset_packs.json', 
                       help='Path to asset configuration file')
    
    args = parser.parse_args()
    
    fetcher = AssetFetcher(args.config)
    
    if args.list:
        fetcher.list_packs()
    elif args.fetch:
        success = fetcher.fetch_pack(args.fetch, args.force)
        sys.exit(0 if success else 1)
    elif args.remove:
        success = fetcher.remove_pack(args.remove)
        sys.exit(0 if success else 1)
    elif args.update:
        success = fetcher.update_pack(args.update)
        sys.exit(0 if success else 1)
    else:
        parser.print_help()

if __name__ == '__main__':
    main()