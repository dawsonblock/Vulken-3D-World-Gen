#!/usr/bin/env python3
"""
Cleanup script for CMake build artifacts and generated files.
This script helps maintain a clean build environment by removing
temporary files, build artifacts, and generated content.
"""

import os
import sys
import shutil
import argparse


def find_cmake_build_dirs(root_dir):
    """Find all CMake build directories."""
    build_dirs = []
    for item in os.listdir(root_dir):
        item_path = os.path.join(root_dir, item)
        if os.path.isdir(item_path):
            if (item.startswith('build') or
                    item.startswith('build_') or
                    item == 'cmake-build-debug' or
                    item == 'cmake-build-release'):
                build_dirs.append(item_path)
    return build_dirs


def find_generated_files(root_dir):
    """Find generated files that should be cleaned."""
    generated_files = []

    # Common generated file patterns
    patterns = [
        'CMakeCache.txt',
        'CMakeFiles',
        'cmake_install.cmake',
        'CTestTestfile.cmake',
        'install_manifest.txt',
        'compile_commands.json',
        'Makefile',
        '*.ninja',
        '*.ninja_log',
        '*.ninja_deps'
    ]

    # Cache build directories to avoid repeated calls
    cached_build_dirs = find_cmake_build_dirs(root_dir)

    for root, _, files in os.walk(root_dir):
        # Skip build directories as they're handled separately
        if any(build_dir in root for build_dir in cached_build_dirs):
            continue

        for file in files:
            if any(file.endswith(pattern.replace('*', ''))
               for pattern in patterns):
                generated_files.append(os.path.join(root, file))

    return generated_files


def cleanup_build_dirs(build_dirs, dry_run=False):
    """Remove CMake build directories."""
    removed = []
    for build_dir in build_dirs:
        if os.path.exists(build_dir) and os.path.isdir(build_dir):
            if dry_run:
                print(f"[DRY RUN] Would remove: {build_dir}")
                removed.append(build_dir)
            else:
                try:
                    shutil.rmtree(build_dir)
                    print(f"Removed: {build_dir}")
                    removed.append(build_dir)
                except (OSError, PermissionError) as e:
                    print(f"Error removing {build_dir}: {e}")
    return removed


def cleanup_generated_files(generated_files, dry_run=False):
    """Remove generated files."""
    removed = []
    for file_path in generated_files:
        if os.path.exists(file_path):
            if dry_run:
                print(f"[DRY RUN] Would remove: {file_path}")
                removed.append(file_path)
            else:
                try:
                    if os.path.isdir(file_path):
                        shutil.rmtree(file_path)
                    else:
                        os.remove(file_path)
                    print(f"Removed: {file_path}")
                    removed.append(file_path)
                except (OSError, PermissionError) as e:
                    print(f"Error removing {file_path}: {e}")
    return removed


def cleanup_vcpkg_artifacts(root_dir, dry_run=False):
    """Clean up vcpkg artifacts."""
    vcpkg_dirs = [
        os.path.join(root_dir, 'vcpkg_installed'),
        os.path.join(root_dir, 'vcpkg', 'downloads'),
        os.path.join(root_dir, 'vcpkg', 'buildtrees'),
        os.path.join(root_dir, 'vcpkg', 'packages')
    ]

    removed = []
    found_vcpkg = []
    for vcpkg_dir in vcpkg_dirs:
        if os.path.exists(vcpkg_dir) and os.path.isdir(vcpkg_dir):
            found_vcpkg.append(vcpkg_dir)
            if dry_run:
                print(f"[DRY RUN] Would remove: {vcpkg_dir}")
                removed.append(vcpkg_dir)
            else:
                try:
                    shutil.rmtree(vcpkg_dir)
                    print(f"Removed: {vcpkg_dir}")
                    removed.append(vcpkg_dir)
                except (OSError, PermissionError) as e:
                    print(f"Error removing {vcpkg_dir}: {e}")
    return removed, found_vcpkg


def main():
    parser = argparse.ArgumentParser(
        description='Clean up CMake build artifacts')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would be removed without actually '
                             'removing')
    parser.add_argument('--build-dirs-only', action='store_true',
                        help='Only clean build directories')
    parser.add_argument('--generated-only', action='store_true',
                        help='Only clean generated files')
    parser.add_argument('--vcpkg-only', action='store_true',
                        help='Only clean vcpkg artifacts')
    parser.add_argument('--root-dir', default='.',
                        help='Root directory to clean '
                        '(default: current directory)')

    args = parser.parse_args()

    root_dir = os.path.abspath(args.root_dir)
    print(f"Cleaning CMake artifacts in: {root_dir}")

    if args.dry_run:
        print("DRY RUN MODE - No files will be actually removed")

    total_removed = 0

    # Clean build directories
    if not args.generated_only and not args.vcpkg_only:
        print("\n=== Cleaning Build Directories ===")
        build_dirs = find_cmake_build_dirs(root_dir)
        removed_dirs = cleanup_build_dirs(build_dirs, args.dry_run)
        total_removed += len(removed_dirs)
        print(f"Found {len(build_dirs)} build directories")

    # Clean generated files
    if not args.build_dirs_only and not args.vcpkg_only:
        print("\n=== Cleaning Generated Files ===")
        generated_files = find_generated_files(root_dir)
        removed_files = cleanup_generated_files(generated_files, args.dry_run)
        total_removed += len(removed_files)
        print(f"Found {len(generated_files)} generated files")

    # Clean vcpkg artifacts
    if not args.build_dirs_only and not args.generated_only:
        print("\n=== Cleaning vcpkg Artifacts ===")
        removed_vcpkg, found_vcpkg = cleanup_vcpkg_artifacts(root_dir,
                                                             args.dry_run)
        total_removed += len(removed_vcpkg)
        print(f"Found {len(found_vcpkg)} vcpkg artifacts")

    print("\n=== Cleanup Complete ===")
    if args.dry_run:
        print(f"Would remove {total_removed} items")
    else:
        print(f"Removed {total_removed} items")

    return 0


if __name__ == '__main__':
    sys.exit(main())
