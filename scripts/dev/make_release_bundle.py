#!/usr/bin/env python3
"""
Vulken-3D Release Bundle Creator
================================

Creates production-ready release bundles for different platforms.
Includes binaries, assets, configuration, and documentation.
"""

import os
import sys
import shutil
import subprocess
import zipfile
import tarfile
import json
import hashlib
from pathlib import Path
from typing import Dict, List, Optional
import argparse
import platform


class ReleaseBundler:
    def __init__(self, build_dir: str = "build", output_dir: str = "release"):
        self.build_dir = Path(build_dir)
        self.output_dir = Path(output_dir)
        self.repo_root = Path(".")
        
        # Platform detection
        self.platform_name = platform.system().lower()
        self.arch = platform.machine().lower()
        if self.arch == "x86_64":
            self.arch = "x64"
        elif self.arch == "aarch64":
            self.arch = "arm64"
        
        # Version information
        self.version = self.get_version()
        self.commit_hash = self.get_git_commit()
        
        # Bundle metadata
        self.bundle_info = {
            "name": "vulken-3d-world-gen",
            "version": self.version,
            "commit": self.commit_hash,
            "platform": self.platform_name,
            "architecture": self.arch,
            "build_date": None,  # Will be set during bundle creation
            "components": [],
            "checksums": {}
        }

    def get_version(self) -> str:
        """Get version from git tags or default."""
        try:
            result = subprocess.run(
                ["git", "describe", "--tags", "--always", "--dirty"],
                capture_output=True, text=True, check=True
            )
            return result.stdout.strip()
        except:
            return "0.9.0-prodp1"

    def get_git_commit(self) -> str:
        """Get current git commit hash."""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True, text=True, check=True
            )
            return result.stdout.strip()[:8]
        except:
            return "unknown"

    def calculate_file_hash(self, filepath: Path) -> str:
        """Calculate SHA256 hash of a file."""
        hash_sha256 = hashlib.sha256()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()

    def copy_with_hash(self, src: Path, dst: Path) -> None:
        """Copy file and record its hash."""
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        
        # Calculate hash for verification
        file_hash = self.calculate_file_hash(dst)
        relative_path = dst.relative_to(self.bundle_dir)
        self.bundle_info["checksums"][str(relative_path)] = file_hash

    def create_bundle_structure(self, bundle_name: str) -> Path:
        """Create the basic bundle directory structure."""
        self.bundle_dir = self.output_dir / bundle_name
        
        # Remove existing bundle
        if self.bundle_dir.exists():
            shutil.rmtree(self.bundle_dir)
        
        # Create directory structure
        directories = [
            "bin",
            "config", 
            "assets",
            "docs",
            "scripts",
            "shaders_cache",
            "logs"
        ]
        
        for directory in directories:
            (self.bundle_dir / directory).mkdir(parents=True, exist_ok=True)
        
        return self.bundle_dir

    def copy_binaries(self) -> None:
        """Copy built executables to bundle."""
        print("📦 Copying binaries...")
        
        apps_dir = self.build_dir / "apps"
        if not apps_dir.exists():
            print(f"⚠️  Apps directory not found: {apps_dir}")
            return
        
        binaries = []
        
        # Define expected binaries
        binary_names = [
            "smoke_graphics_headless",
            "operator_console", 
            "gui_fullscreen_demo",
            "weather_demo",
            "p1_memory_demo"
        ]
        
        # Copy binaries based on platform
        if self.platform_name == "windows":
            binary_extension = ".exe"
        else:
            binary_extension = ""
        
        for binary_name in binary_names:
            src_path = apps_dir / (binary_name + binary_extension)
            if src_path.exists():
                dst_path = self.bundle_dir / "bin" / (binary_name + binary_extension)
                self.copy_with_hash(src_path, dst_path)
                binaries.append(binary_name)
                print(f"  ✅ {binary_name}")
                
                # Make executable on Unix systems
                if self.platform_name != "windows":
                    os.chmod(dst_path, 0o755)
            else:
                print(f"  ❌ Missing: {binary_name}")
        
        self.bundle_info["components"].append({
            "name": "binaries",
            "count": len(binaries),
            "items": binaries
        })

    def copy_assets(self) -> None:
        """Copy game assets to bundle."""
        print("🎨 Copying assets...")
        
        assets_dir = self.repo_root / "assets"
        if assets_dir.exists():
            # Copy entire assets directory
            for item in assets_dir.rglob("*"):
                if item.is_file():
                    relative_path = item.relative_to(assets_dir)
                    dst_path = self.bundle_dir / "assets" / relative_path
                    self.copy_with_hash(item, dst_path)
            
            asset_count = len(list((self.bundle_dir / "assets").rglob("*")))
            print(f"  ✅ Copied {asset_count} asset files")
            
            self.bundle_info["components"].append({
                "name": "assets",
                "count": asset_count
            })
        else:
            print("  ⚠️  No assets directory found")

    def copy_configuration(self) -> None:
        """Copy configuration files to bundle."""
        print("⚙️  Copying configuration...")
        
        config_dir = self.repo_root / "config"
        if config_dir.exists():
            config_files = []
            for config_file in config_dir.glob("*.yaml"):
                dst_path = self.bundle_dir / "config" / config_file.name
                self.copy_with_hash(config_file, dst_path)
                config_files.append(config_file.name)
                print(f"  ✅ {config_file.name}")
            
            self.bundle_info["components"].append({
                "name": "configuration",
                "count": len(config_files),
                "items": config_files
            })
        else:
            print("  ⚠️  No config directory found")

    def copy_shaders(self) -> None:
        """Copy compiled shaders to bundle."""
        print("🔧 Copying shaders...")
        
        shader_cache_dir = self.build_dir / "shaders_cache"
        if shader_cache_dir.exists():
            shader_count = 0
            for shader_file in shader_cache_dir.rglob("*"):
                if shader_file.is_file():
                    relative_path = shader_file.relative_to(shader_cache_dir)
                    dst_path = self.bundle_dir / "shaders_cache" / relative_path
                    self.copy_with_hash(shader_file, dst_path)
                    shader_count += 1
            
            print(f"  ✅ Copied {shader_count} shader files")
            
            self.bundle_info["components"].append({
                "name": "shaders",
                "count": shader_count
            })
        else:
            print("  ⚠️  No shader cache found")

    def copy_documentation(self) -> None:
        """Copy documentation to bundle."""
        print("📚 Copying documentation...")
        
        doc_files = [
            ("RUN.md", "Quick start guide"),
            ("README.md", "Project overview"),
            ("LICENSE", "License information"),
            ("docs/ARCHITECTURE.md", "Engine architecture"),
            ("docs/OPERATOR_GUIDE.md", "Operator guide"),
            ("docs/AI_PIPELINE.md", "AI pipeline documentation")
        ]
        
        copied_docs = []
        for doc_file, description in doc_files:
            src_path = self.repo_root / doc_file
            if src_path.exists():
                dst_path = self.bundle_dir / "docs" / Path(doc_file).name
                self.copy_with_hash(src_path, dst_path)
                copied_docs.append(Path(doc_file).name)
                print(f"  ✅ {Path(doc_file).name}")
            else:
                print(f"  ❌ Missing: {doc_file}")
        
        self.bundle_info["components"].append({
            "name": "documentation",
            "count": len(copied_docs),
            "items": copied_docs
        })

    def copy_scripts(self) -> None:
        """Copy utility scripts to bundle."""
        print("📜 Copying scripts...")
        
        # Copy essential scripts
        script_patterns = [
            "scripts/bench/*.py",
            "scripts/data/*.py", 
            "scripts/shaders/*.py"
        ]
        
        copied_scripts = []
        for pattern in script_patterns:
            for script_file in self.repo_root.glob(pattern):
                if script_file.is_file():
                    # Preserve directory structure
                    relative_path = script_file.relative_to(self.repo_root / "scripts")
                    dst_path = self.bundle_dir / "scripts" / relative_path
                    self.copy_with_hash(script_file, dst_path)
                    copied_scripts.append(str(relative_path))
                    
                    # Make executable on Unix systems
                    if self.platform_name != "windows" and script_file.suffix == ".py":
                        os.chmod(dst_path, 0o755)
        
        print(f"  ✅ Copied {len(copied_scripts)} scripts")
        
        self.bundle_info["components"].append({
            "name": "scripts",
            "count": len(copied_scripts),
            "items": copied_scripts
        })

    def create_run_script(self) -> None:
        """Create platform-specific run script."""
        print("🚀 Creating run script...")
        
        if self.platform_name == "windows":
            run_script = """@echo off
REM Vulken-3D Quick Start Script
echo Starting Vulken-3D Engine...

REM Set environment variables
set VULKAN_SDK=%CD%
set PATH=%CD%\\bin;%PATH%

REM Run the headless demo
echo Running headless graphics demo...
bin\\smoke_graphics_headless.exe --config config\\engine.yaml --headless

REM Optional: Run operator console
REM bin\\operator_console.exe

echo.
echo Press any key to exit...
pause > nul
"""
            script_name = "run.bat"
        else:
            run_script = """#!/bin/bash
# Vulken-3D Quick Start Script
echo "Starting Vulken-3D Engine..."

# Set environment variables
export VULKAN_SDK="${PWD}"
export PATH="${PWD}/bin:${PATH}"

# Make binaries executable
chmod +x bin/*

# Run the headless demo
echo "Running headless graphics demo..."
./bin/smoke_graphics_headless --config config/engine.yaml --headless

# Optional: Run operator console
# ./bin/operator_console

echo "Done."
"""
            script_name = "run.sh"
        
        script_path = self.bundle_dir / script_name
        with open(script_path, 'w', newline='') as f:
            f.write(run_script)
        
        # Make executable on Unix systems
        if self.platform_name != "windows":
            os.chmod(script_path, 0o755)
        
        print(f"  ✅ Created {script_name}")

    def create_release_notes(self) -> None:
        """Create release notes file."""
        print("📝 Creating release notes...")
        
        release_notes = f"""# Vulken-3D Release {self.version}

## Build Information
- **Version**: {self.version}
- **Commit**: {self.commit_hash}  
- **Platform**: {self.platform_name}
- **Architecture**: {self.arch}
- **Build Date**: {self.bundle_info['build_date']}

## What's Included

"""
        
        # Add component information
        for component in self.bundle_info['components']:
            release_notes += f"### {component['name'].title()}\n"
            release_notes += f"- **Count**: {component['count']} items\n"
            if 'items' in component:
                release_notes += f"- **Items**: {', '.join(component['items'][:5])}"
                if len(component['items']) > 5:
                    release_notes += f" (and {len(component['items']) - 5} more)"
                release_notes += "\n"
            release_notes += "\n"

        release_notes += """## Quick Start

1. Extract this bundle to your desired location
2. Run the platform-specific script:
   - **Windows**: `run.bat`  
   - **Linux/macOS**: `./run.sh`

## Available Applications

- **smoke_graphics_headless**: Headless graphics demonstration
- **operator_console**: Real-time monitoring and control interface
- **gui_fullscreen_demo**: Full-screen GUI demonstration
- **weather_demo**: Weather system showcase

## Configuration

Configuration files are located in the `config/` directory:
- `engine.yaml`: Core engine settings
- `renderer.yaml`: Rendering pipeline configuration  
- `datasets.yaml`: Asset storage configuration
- `weather.yaml`: Weather system parameters

## Documentation

See the `docs/` directory for detailed documentation:
- `RUN.md`: Quick start guide
- `ARCHITECTURE.md`: Engine architecture overview
- `OPERATOR_GUIDE.md`: Operator console guide

## Troubleshooting

### Vulkan Driver Issues
Ensure you have up-to-date Vulkan drivers installed:
- **NVIDIA**: Download latest drivers from nvidia.com
- **AMD**: Install latest Adrenalin drivers
- **Intel**: Update integrated graphics drivers

### Performance Issues
- Adjust render distance in configuration
- Disable expensive post-processing effects
- Check GPU memory usage

### Asset Loading Issues
- Ensure all files were extracted properly
- Check file permissions (Linux/macOS)
- Verify asset integrity using provided checksums

## Support

For issues and support:
- Check the documentation in `docs/`
- Review the configuration options in `config/`
- Enable verbose logging by setting `log_level: "DEBUG"`

---

*This release was automatically generated by the Vulken-3D build system.*
"""
        
        notes_path = self.bundle_dir / "RELEASE_NOTES.md"
        with open(notes_path, 'w') as f:
            f.write(release_notes)
        
        print("  ✅ Created RELEASE_NOTES.md")

    def create_bundle_manifest(self) -> None:
        """Create bundle manifest with checksums."""
        print("📋 Creating manifest...")
        
        import datetime
        self.bundle_info["build_date"] = datetime.datetime.utcnow().isoformat() + "Z"
        
        # Add file count
        total_files = len(self.bundle_info["checksums"])
        self.bundle_info["total_files"] = total_files
        
        manifest_path = self.bundle_dir / "manifest.json"
        with open(manifest_path, 'w') as f:
            json.dump(self.bundle_info, f, indent=2)
        
        print(f"  ✅ Created manifest with {total_files} files")

    def create_archive(self, bundle_name: str, format: str = "zip") -> Path:
        """Create compressed archive of the bundle."""
        print(f"📦 Creating {format.upper()} archive...")
        
        if format == "zip":
            archive_path = self.output_dir / f"{bundle_name}.zip"
            with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                for file_path in self.bundle_dir.rglob('*'):
                    if file_path.is_file():
                        arc_name = file_path.relative_to(self.output_dir)
                        zipf.write(file_path, arc_name)
        
        elif format == "tar.gz":
            archive_path = self.output_dir / f"{bundle_name}.tar.gz"
            with tarfile.open(archive_path, 'w:gz') as tarf:
                tarf.add(self.bundle_dir, arcname=bundle_name)
        
        else:
            raise ValueError(f"Unsupported archive format: {format}")
        
        # Calculate archive hash
        archive_hash = self.calculate_file_hash(archive_path)
        
        # Create checksum file
        checksum_path = archive_path.with_suffix(archive_path.suffix + ".sha256")
        with open(checksum_path, 'w') as f:
            f.write(f"{archive_hash}  {archive_path.name}\n")
        
        file_size_mb = archive_path.stat().st_size / (1024 * 1024)
        print(f"  ✅ Created {archive_path.name} ({file_size_mb:.1f} MB)")
        print(f"  ✅ SHA256: {archive_hash[:16]}...")
        
        return archive_path

    def create_release_bundle(self, format: str = "zip") -> Path:
        """Create complete release bundle."""
        print(f"🎯 Creating Vulken-3D release bundle for {self.platform_name}-{self.arch}")
        print(f"Version: {self.version} (commit: {self.commit_hash})")
        print()
        
        # Generate bundle name
        bundle_name = f"vulken3d_{self.platform_name}_{self.arch}_{self.commit_hash}"
        
        # Create bundle directory
        self.create_bundle_structure(bundle_name)
        
        # Copy components
        self.copy_binaries()
        self.copy_assets()
        self.copy_configuration()
        self.copy_shaders()
        self.copy_documentation()
        self.copy_scripts()
        
        # Create additional files
        self.create_run_script()
        self.create_release_notes()
        self.create_bundle_manifest()
        
        # Create archive
        archive_path = self.create_archive(bundle_name, format)
        
        print()
        print("✅ Release bundle created successfully!")
        print(f"📦 Bundle: {archive_path}")
        print(f"📊 Total files: {self.bundle_info['total_files']}")
        
        # Print component summary
        print("\n📋 Components:")
        for component in self.bundle_info['components']:
            print(f"  - {component['name'].title()}: {component['count']} items")
        
        return archive_path


def main():
    parser = argparse.ArgumentParser(description="Create Vulken-3D release bundle")
    parser.add_argument("--build-dir", default="build", help="Build directory")
    parser.add_argument("--output-dir", default="release", help="Output directory")
    parser.add_argument("--format", choices=["zip", "tar.gz"], default="zip", 
                       help="Archive format")
    parser.add_argument("--clean", action="store_true", 
                       help="Clean output directory before creating bundle")
    
    args = parser.parse_args()
    
    # Clean output directory if requested
    if args.clean and Path(args.output_dir).exists():
        shutil.rmtree(args.output_dir)
        print(f"🧹 Cleaned output directory: {args.output_dir}")
    
    # Create output directory
    Path(args.output_dir).mkdir(parents=True, exist_ok=True)
    
    # Create release bundle
    bundler = ReleaseBundler(args.build_dir, args.output_dir)
    
    try:
        archive_path = bundler.create_release_bundle(args.format)
        print(f"\n🎉 Release bundle ready: {archive_path}")
        return 0
    except Exception as e:
        print(f"\n❌ Failed to create release bundle: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())