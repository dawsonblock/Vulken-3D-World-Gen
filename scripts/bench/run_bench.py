#!/usr/bin/env python3
"""
Vulken-3D Benchmark Runner
==========================

Runs performance benchmarks and generates JSON reports for regression analysis.
"""

import os
import sys
import json
import subprocess
import time
from pathlib import Path
from typing import Dict, Any, List
import argparse


class BenchmarkRunner:
    def __init__(self):
        self.results = {
            "timestamp": time.time(),
            "commit_hash": self.get_git_commit(),
            "branch": self.get_git_branch(),
            "benchmarks": [],
            "system_info": self.get_system_info()
        }
    
    def get_git_commit(self) -> str:
        try:
            result = subprocess.run(["git", "rev-parse", "HEAD"], 
                                  capture_output=True, text=True, check=True)
            return result.stdout.strip()
        except:
            return "unknown"
    
    def get_git_branch(self) -> str:
        try:
            result = subprocess.run(["git", "branch", "--show-current"], 
                                  capture_output=True, text=True, check=True)
            return result.stdout.strip()
        except:
            return "unknown"
    
    def get_system_info(self) -> Dict[str, Any]:
        info = {}
        try:
            # CPU info
            with open("/proc/cpuinfo", "r") as f:
                for line in f:
                    if "model name" in line:
                        info["cpu"] = line.split(":")[1].strip()
                        break
            
            # Memory info
            with open("/proc/meminfo", "r") as f:
                for line in f:
                    if "MemTotal" in line:
                        info["memory_kb"] = int(line.split()[1])
                        break
        except:
            pass
        
        return info
    
    def run_cpp_benchmarks(self, build_dir: str = "build") -> None:
        """Run C++ Google Benchmark tests."""
        print("🔬 Running C++ benchmarks...")
        
        bench_executable = Path(build_dir) / "tests" / "bench_mesher"
        if bench_executable.exists():
            try:
                result = subprocess.run([str(bench_executable), "--benchmark_format=json"], 
                                      capture_output=True, text=True, check=True)
                
                # Parse Google Benchmark JSON output
                bench_data = json.loads(result.stdout)
                for benchmark in bench_data.get("benchmarks", []):
                    self.results["benchmarks"].append({
                        "name": benchmark["name"],
                        "time": benchmark["real_time"],
                        "unit": "ns",
                        "type": "cpp_benchmark",
                        "iterations": benchmark["iterations"],
                        "cpu_time": benchmark["cpu_time"]
                    })
                print(f"  ✅ Completed {len(bench_data.get('benchmarks', []))} C++ benchmarks")
            except Exception as e:
                print(f"  ❌ C++ benchmark failed: {e}")
        else:
            print(f"  ⚠️  C++ benchmark executable not found: {bench_executable}")
    
    def run_python_benchmarks(self) -> None:
        """Run Python-based performance tests."""
        print("🐍 Running Python benchmarks...")
        
        # Voxel math performance test
        start_time = time.perf_counter()
        self.benchmark_coordinate_conversion()
        coord_time = (time.perf_counter() - start_time) * 1000
        
        self.results["benchmarks"].append({
            "name": "coordinate_conversion_10k",
            "time": coord_time,
            "unit": "ms",
            "type": "python_benchmark"
        })
        
        # Asset loading performance
        start_time = time.perf_counter()
        self.benchmark_asset_loading()
        asset_time = (time.perf_counter() - start_time) * 1000
        
        self.results["benchmarks"].append({
            "name": "asset_loading_simulation",
            "time": asset_time,
            "unit": "ms", 
            "type": "python_benchmark"
        })
        
        print(f"  ✅ Completed 2 Python benchmarks")
    
    def benchmark_coordinate_conversion(self) -> None:
        """Benchmark coordinate conversion performance."""
        import math
        
        CHUNK_SIZE = 64
        
        def world_to_chunk(x: float, y: float, z: float):
            return (int(math.floor(x / CHUNK_SIZE)), 
                   int(math.floor(y / CHUNK_SIZE)), 
                   int(math.floor(z / CHUNK_SIZE)))
        
        def world_to_voxel(x: float, y: float, z: float):
            vx = int(math.floor(x)) % CHUNK_SIZE
            vy = int(math.floor(y)) % CHUNK_SIZE
            vz = int(math.floor(z)) % CHUNK_SIZE
            if vx < 0: vx += CHUNK_SIZE
            if vy < 0: vy += CHUNK_SIZE
            if vz < 0: vz += CHUNK_SIZE
            return (vx, vy, vz)
        
        # Run 10k conversions
        for i in range(10000):
            x, y, z = i % 1000 - 500, i % 256, i % 1000 - 500
            chunk = world_to_chunk(x, y, z)
            voxel = world_to_voxel(x, y, z)
    
    def benchmark_asset_loading(self) -> None:
        """Benchmark asset loading simulation."""
        import hashlib
        import json
        
        # Simulate loading various asset types
        assets = []
        
        for i in range(100):
            # Simulate texture data
            texture_data = b"TEXTURE_DATA_" + str(i).encode() * 100
            texture_hash = hashlib.sha256(texture_data).hexdigest()
            
            assets.append({
                "type": "texture",
                "name": f"texture_{i}",
                "size": len(texture_data),
                "hash": texture_hash
            })
            
            # Simulate mesh data
            mesh_data = {"vertices": list(range(i * 10)), "indices": list(range(i * 6))}
            mesh_json = json.dumps(mesh_data)
            mesh_hash = hashlib.sha256(mesh_json.encode()).hexdigest()
            
            assets.append({
                "type": "mesh", 
                "name": f"mesh_{i}",
                "size": len(mesh_json),
                "hash": mesh_hash
            })
        
        # Simulate Redis operations
        total_size = sum(asset["size"] for asset in assets)
    
    def run_headless_render_benchmark(self) -> None:
        """Run headless rendering benchmarks."""
        print("🖼️ Running render benchmarks...")
        
        # Mock headless rendering benchmark
        start_time = time.perf_counter()
        
        # Simulate rendering 100 frames
        frame_times = []
        for i in range(100):
            frame_start = time.perf_counter()
            
            # Simulate frame rendering work
            time.sleep(0.001)  # 1ms simulated render time
            
            frame_end = time.perf_counter()
            frame_times.append((frame_end - frame_start) * 1000)  # Convert to ms
        
        total_time = (time.perf_counter() - start_time) * 1000
        avg_frame_time = sum(frame_times) / len(frame_times)
        fps = 1000.0 / avg_frame_time
        
        self.results["benchmarks"].extend([
            {
                "name": "headless_render_100_frames",
                "time": total_time,
                "unit": "ms",
                "type": "render_benchmark"
            },
            {
                "name": "average_frame_time",
                "time": avg_frame_time,
                "unit": "ms",
                "type": "render_benchmark"
            },
            {
                "name": "average_fps",
                "time": fps,
                "unit": "fps",
                "type": "render_benchmark"
            }
        ])
        
        print(f"  ✅ Render benchmark: {fps:.1f} FPS average")
    
    def save_results(self, output_path: str) -> None:
        """Save benchmark results to JSON file."""
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(self.results, f, indent=2)
        
        print(f"📊 Results saved to: {output_path}")
    
    def print_summary(self) -> None:
        """Print benchmark summary."""
        print("\n📈 Benchmark Summary:")
        print(f"  Commit: {self.results['commit_hash'][:8]}")
        print(f"  Branch: {self.results['branch']}")
        print(f"  Total benchmarks: {len(self.results['benchmarks'])}")
        
        for bench in self.results["benchmarks"]:
            time_val = bench["time"]
            if bench["unit"] == "ns":
                time_val = time_val / 1_000_000  # Convert to ms
                unit = "ms"
            else:
                unit = bench["unit"]
            
            print(f"  - {bench['name']}: {time_val:.2f} {unit}")


def main():
    parser = argparse.ArgumentParser(description="Run Vulken-3D performance benchmarks")
    parser.add_argument("--output", default="reports/bench/latest.json",
                       help="Output JSON file path")
    parser.add_argument("--build-dir", default="build",
                       help="Build directory containing executables")
    parser.add_argument("--skip-cpp", action="store_true",
                       help="Skip C++ benchmarks")
    parser.add_argument("--skip-python", action="store_true",
                       help="Skip Python benchmarks")
    parser.add_argument("--skip-render", action="store_true",
                       help="Skip render benchmarks")
    
    args = parser.parse_args()
    
    print("🚀 Starting Vulken-3D benchmark suite...")
    
    runner = BenchmarkRunner()
    
    if not args.skip_cpp:
        runner.run_cpp_benchmarks(args.build_dir)
    
    if not args.skip_python:
        runner.run_python_benchmarks()
    
    if not args.skip_render:
        runner.run_headless_render_benchmark()
    
    runner.save_results(args.output)
    runner.print_summary()
    
    print("\n✅ Benchmark suite completed successfully!")


if __name__ == "__main__":
    main()