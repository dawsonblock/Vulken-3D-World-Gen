#!/usr/bin/env python3
"""
VoxelVK Image Regression Testing
Compares rendered images using SSIM and other metrics to detect visual regressions.
"""

import os
import sys
import json
import argparse
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import numpy as np
from PIL import Image, ImageChops
import cv2

class ImageRegressionTester:
    def __init__(self, golden_dir: str, test_dir: str, tolerance: float = 0.98):
        self.golden_dir = Path(golden_dir)
        self.test_dir = Path(test_dir)
        self.tolerance = tolerance
        self.results = {
            "version": "1.0",
            "test_results": {},
            "summary": {
                "total_tests": 0,
                "passed": 0,
                "failed": 0,
                "ssim_threshold": tolerance
            }
        }
        
    def calculate_ssim(self, img1: np.ndarray, img2: np.ndarray) -> float:
        """Calculate Structural Similarity Index (SSIM) between two images"""
        try:
            # Convert to grayscale if needed
            if len(img1.shape) == 3:
                img1 = cv2.cvtColor(img1, cv2.COLOR_RGB2GRAY)
            if len(img2.shape) == 3:
                img2 = cv2.cvtColor(img2, cv2.COLOR_RGB2GRAY)
            
            # Calculate SSIM
            ssim = cv2.matchTemplate(img1, img2, cv2.TM_CCOEFF_NORMED)[0][0]
            return float(ssim)
        except Exception as e:
            print(f"Error calculating SSIM: {e}")
            return 0.0
    
    def calculate_psnr(self, img1: np.ndarray, img2: np.ndarray) -> float:
        """Calculate Peak Signal-to-Noise Ratio (PSNR) between two images"""
        try:
            # Calculate MSE
            mse = np.mean((img1.astype(float) - img2.astype(float)) ** 2)
            if mse == 0:
                return float('inf')
            
            # Calculate PSNR
            max_pixel = 255.0
            psnr = 20 * np.log10(max_pixel / np.sqrt(mse))
            return float(psnr)
        except Exception as e:
            print(f"Error calculating PSNR: {e}")
            return 0.0
    
    def calculate_mse(self, img1: np.ndarray, img2: np.ndarray) -> float:
        """Calculate Mean Squared Error (MSE) between two images"""
        try:
            mse = np.mean((img1.astype(float) - img2.astype(float)) ** 2)
            return float(mse)
        except Exception as e:
            print(f"Error calculating MSE: {e}")
            return float('inf')
    
    def load_image(self, path: Path) -> Optional[np.ndarray]:
        """Load image and convert to numpy array"""
        try:
            if not path.exists():
                return None
            
            # Load with PIL first to handle various formats
            pil_img = Image.open(path)
            if pil_img.mode != 'RGB':
                pil_img = pil_img.convert('RGB')
            
            # Convert to numpy array
            img_array = np.array(pil_img)
            return img_array
        except Exception as e:
            print(f"Error loading image {path}: {e}")
            return None
    
    def compare_images(self, golden_path: Path, test_path: Path) -> Dict:
        """Compare two images and return detailed metrics"""
        golden_img = self.load_image(golden_path)
        test_img = self.load_image(test_path)
        
        if golden_img is None or test_img is None:
            return {
                "error": "Failed to load one or both images",
                "passed": False
            }
        
        # Ensure images have the same dimensions
        if golden_img.shape != test_img.shape:
            return {
                "error": f"Image dimensions mismatch: {golden_img.shape} vs {test_img.shape}",
                "passed": False
            }
        
        # Calculate metrics
        ssim = self.calculate_ssim(golden_img, test_img)
        psnr = self.calculate_psnr(golden_img, test_img)
        mse = self.calculate_mse(golden_img, test_img)
        
        # Determine if test passed
        passed = ssim >= self.tolerance
        
        return {
            "ssim": ssim,
            "psnr": psnr,
            "mse": mse,
            "passed": passed,
            "tolerance": self.tolerance,
            "golden_path": str(golden_path),
            "test_path": str(test_path)
        }
    
    def find_test_cases(self) -> List[Tuple[Path, Path]]:
        """Find all test cases by looking for corresponding golden and test images"""
        test_cases = []
        
        # Look for golden images
        golden_images = list(self.golden_dir.glob("*.png")) + list(self.golden_dir.glob("*.jpg"))
        
        for golden_path in golden_images:
            test_path = self.test_dir / golden_path.name
            
            if test_path.exists():
                test_cases.append((golden_path, test_path))
            else:
                print(f"Warning: No test image found for {golden_path.name}")
        
        return test_cases
    
    def run_tests(self) -> bool:
        """Run all image regression tests"""
        print("VoxelVK Image Regression Testing")
        print("=" * 40)
        print(f"Golden directory: {self.golden_dir}")
        print(f"Test directory: {self.test_dir}")
        print(f"SSIM tolerance: {self.tolerance}")
        print()
        
        # Find test cases
        test_cases = self.find_test_cases()
        
        if not test_cases:
            print("No test cases found!")
            return False
        
        print(f"Found {len(test_cases)} test cases")
        print()
        
        # Run each test
        all_passed = True
        
        for golden_path, test_path in test_cases:
            test_name = golden_path.stem
            print(f"Testing: {test_name}")
            
            result = self.compare_images(golden_path, test_path)
            self.results["test_results"][test_name] = result
            
            if result.get("passed", False):
                print(f"  ✓ PASS - SSIM: {result['ssim']:.4f}, PSNR: {result['psnr']:.2f}dB")
                self.results["summary"]["passed"] += 1
            else:
                print(f"  ✗ FAIL - SSIM: {result.get('ssim', 0):.4f}, PSNR: {result.get('psnr', 0):.2f}dB")
                if "error" in result:
                    print(f"    Error: {result['error']}")
                self.results["summary"]["failed"] += 1
                all_passed = False
            
            print()
        
        # Update summary
        self.results["summary"]["total_tests"] = len(test_cases)
        
        # Print summary
        print("Test Summary:")
        print(f"  Total tests: {self.results['summary']['total_tests']}")
        print(f"  Passed: {self.results['summary']['passed']}")
        print(f"  Failed: {self.results['summary']['failed']}")
        print(f"  Success rate: {self.results['summary']['passed'] / self.results['summary']['total_tests'] * 100:.1f}%")
        
        return all_passed
    
    def save_report(self, output_path: str) -> None:
        """Save test results to JSON report"""
        with open(output_path, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"Report saved to: {output_path}")
    
    def generate_diff_images(self, output_dir: str) -> None:
        """Generate difference images for failed tests"""
        diff_dir = Path(output_dir)
        diff_dir.mkdir(parents=True, exist_ok=True)
        
        for test_name, result in self.results["test_results"].items():
            if not result.get("passed", False) and "error" not in result:
                try:
                    golden_img = self.load_image(Path(result["golden_path"]))
                    test_img = self.load_image(Path(result["test_path"]))
                    
                    if golden_img is not None and test_img is not None:
                        # Calculate difference
                        diff_img = np.abs(golden_img.astype(float) - test_img.astype(float))
                        diff_img = np.clip(diff_img, 0, 255).astype(np.uint8)
                        
                        # Save difference image
                        diff_path = diff_dir / f"{test_name}_diff.png"
                        Image.fromarray(diff_img).save(diff_path)
                        print(f"Difference image saved: {diff_path}")
                        
                except Exception as e:
                    print(f"Error generating diff image for {test_name}: {e}")

def main():
    parser = argparse.ArgumentParser(description='VoxelVK Image Regression Testing')
    parser.add_argument('--golden-dir', default='tests/render_diff/golden',
                        help='Directory containing golden reference images')
    parser.add_argument('--test-dir', default='tests/render_diff/test',
                        help='Directory containing test images to compare')
    parser.add_argument('--tolerance', type=float, default=0.98,
                        help='SSIM tolerance threshold (default: 0.98)')
    parser.add_argument('--output-report', default='tests/render_diff/regression_report.json',
                        help='Output path for test report')
    parser.add_argument('--generate-diffs', action='store_true',
                        help='Generate difference images for failed tests')
    parser.add_argument('--diff-dir', default='tests/render_diff/diffs',
                        help='Directory for difference images')
    
    args = parser.parse_args()
    
    # Create test directories if they don't exist
    Path(args.golden_dir).mkdir(parents=True, exist_ok=True)
    Path(args.test_dir).mkdir(parents=True, exist_ok=True)
    
    # Run tests
    tester = ImageRegressionTester(args.golden_dir, args.test_dir, args.tolerance)
    success = tester.run_tests()
    
    # Save report
    tester.save_report(args.output_report)
    
    # Generate diff images if requested
    if args.generate_diffs:
        tester.generate_diff_images(args.diff_dir)
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()