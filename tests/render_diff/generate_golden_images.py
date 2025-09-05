#!/usr/bin/env python3
"""
Generate golden reference images for image regression testing
"""

import os
import sys
import subprocess
from pathlib import Path

def generate_golden_images():
    """Generate golden reference images using the minimal renderer"""
    
    # Ensure golden directory exists
    golden_dir = Path("tests/render_diff/golden")
    golden_dir.mkdir(parents=True, exist_ok=True)
    
    # Test cases to generate
    test_cases = [
        ("basic_triangle", "basic_triangle.png"),
        ("gradient", "gradient.png"),
        ("checkerboard", "checkerboard.png"),
        ("default", "default.png")
    ]
    
    print("Generating golden reference images...")
    
    for test_name, filename in test_cases:
        output_path = golden_dir / filename
        print(f"Generating {test_name} -> {output_path}")
        
        # Build the minimal renderer if it doesn't exist
        renderer_path = "build/tests/render_diff/minimal_renderer"
        if not Path(renderer_path).exists():
            print("Building minimal renderer...")
            result = subprocess.run(["cmake", "--build", "build", "--target", "minimal_renderer"], 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                print(f"Failed to build minimal renderer: {result.stderr}")
                continue
        
        # Generate the image
        result = subprocess.run([renderer_path, test_name, str(output_path), "800", "600"],
                              capture_output=True, text=True)
        if result.returncode == 0:
            print(f"  ✓ Generated {filename}")
        else:
            print(f"  ✗ Failed to generate {filename}: {result.stderr}")
    
    print("Golden image generation complete!")

if __name__ == "__main__":
    generate_golden_images()