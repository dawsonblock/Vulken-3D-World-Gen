#!/usr/bin/env python3
"""
Benchmark Regression Analysis
=============================

Compares current benchmark results against baseline to detect performance regressions.
"""

import json
import sys
import argparse
from typing import Dict, Any, List, Optional


def load_benchmark_results(filepath: str) -> Dict[str, Any]:
    """Load benchmark results from JSON file."""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        return {"benchmarks": []}
    except json.JSONDecodeError as e:
        print(f"Error parsing {filepath}: {e}")
        return {"benchmarks": []}


def compare_benchmarks(current: Dict[str, Any], baseline: Dict[str, Any], 
                      threshold: float = 0.10) -> Dict[str, Any]:
    """Compare current results against baseline."""
    
    current_benchmarks = {b["name"]: b for b in current.get("benchmarks", [])}
    baseline_benchmarks = {b["name"]: b for b in baseline.get("benchmarks", [])}
    
    report = {
        "summary": {
            "total_benchmarks": len(current_benchmarks),
            "baseline_benchmarks": len(baseline_benchmarks),
            "regressions": 0,
            "improvements": 0,
            "unchanged": 0,
            "new_benchmarks": 0,
            "missing_benchmarks": 0
        },
        "regressions": [],
        "improvements": [],
        "new_benchmarks": [],
        "missing_benchmarks": []
    }
    
    # Check each current benchmark against baseline
    for name, current_bench in current_benchmarks.items():
        if name not in baseline_benchmarks:
            report["new_benchmarks"].append({
                "name": name,
                "time": current_bench["time"],
                "unit": current_bench["unit"]
            })
            report["summary"]["new_benchmarks"] += 1
            continue
        
        baseline_bench = baseline_benchmarks[name]
        
        # Calculate relative change
        current_time = current_bench["time"]
        baseline_time = baseline_bench["time"]
        
        if baseline_time == 0:
            continue  # Skip division by zero
        
        relative_change = (current_time - baseline_time) / baseline_time
        
        # Determine if this is a regression, improvement, or within tolerance
        if relative_change > threshold:
            # Regression - current is slower
            report["regressions"].append({
                "name": name,
                "current_time": current_time,
                "baseline_time": baseline_time,
                "relative_change": relative_change,
                "absolute_change": current_time - baseline_time,
                "unit": current_bench["unit"]
            })
            report["summary"]["regressions"] += 1
            
        elif relative_change < -threshold:
            # Improvement - current is faster
            report["improvements"].append({
                "name": name,
                "current_time": current_time,
                "baseline_time": baseline_time,
                "relative_change": relative_change,
                "absolute_change": current_time - baseline_time,
                "unit": current_bench["unit"]
            })
            report["summary"]["improvements"] += 1
            
        else:
            # Within tolerance
            report["summary"]["unchanged"] += 1
    
    # Check for benchmarks that exist in baseline but not in current
    for name, baseline_bench in baseline_benchmarks.items():
        if name not in current_benchmarks:
            report["missing_benchmarks"].append({
                "name": name,
                "baseline_time": baseline_bench["time"],
                "unit": baseline_bench["unit"]
            })
            report["summary"]["missing_benchmarks"] += 1
    
    return report


def print_regression_report(report: Dict[str, Any], threshold: float) -> None:
    """Print human-readable regression report."""
    summary = report["summary"]
    
    print("🔍 Performance Regression Analysis")
    print("=" * 50)
    print(f"Threshold: ±{threshold*100:.1f}%")
    print(f"Total benchmarks: {summary['total_benchmarks']}")
    print(f"Baseline benchmarks: {summary['baseline_benchmarks']}")
    print()
    
    # Summary
    print("📊 Summary:")
    print(f"  🔴 Regressions: {summary['regressions']}")
    print(f"  🟢 Improvements: {summary['improvements']}")
    print(f"  ⚪ Unchanged: {summary['unchanged']}")
    print(f"  🆕 New: {summary['new_benchmarks']}")
    print(f"  ❌ Missing: {summary['missing_benchmarks']}")
    print()
    
    # Regressions (detailed)
    if report["regressions"]:
        print("🔴 Performance Regressions:")
        for reg in report["regressions"]:
            change_pct = reg["relative_change"] * 100
            print(f"  - {reg['name']}: {reg['baseline_time']:.2f} → {reg['current_time']:.2f} {reg['unit']} "
                  f"({change_pct:+.1f}%)")
        print()
    
    # Improvements (detailed)
    if report["improvements"]:
        print("🟢 Performance Improvements:")
        for imp in report["improvements"]:
            change_pct = imp["relative_change"] * 100
            print(f"  - {imp['name']}: {imp['baseline_time']:.2f} → {imp['current_time']:.2f} {imp['unit']} "
                  f"({change_pct:+.1f}%)")
        print()
    
    # New benchmarks
    if report["new_benchmarks"]:
        print("🆕 New Benchmarks:")
        for new in report["new_benchmarks"]:
            print(f"  - {new['name']}: {new['time']:.2f} {new['unit']}")
        print()
    
    # Missing benchmarks
    if report["missing_benchmarks"]:
        print("❌ Missing Benchmarks:")
        for missing in report["missing_benchmarks"]:
            print(f"  - {missing['name']}: {missing['baseline_time']:.2f} {missing['unit']}")
        print()


def generate_github_comment(report: Dict[str, Any], threshold: float) -> str:
    """Generate GitHub PR comment with regression analysis."""
    summary = report["summary"]
    
    comment = f"## 📊 Performance Regression Analysis\n\n"
    comment += f"**Threshold**: ±{threshold*100:.1f}%\n"
    comment += f"**Total Benchmarks**: {summary['total_benchmarks']}\n\n"
    
    # Status indicator
    if summary["regressions"] > 0:
        comment += f"⚠️ **{summary['regressions']} performance regression(s) detected!**\n\n"
    elif summary["improvements"] > 0:
        comment += f"✅ **Performance improved** ({summary['improvements']} benchmarks faster)\n\n"
    else:
        comment += f"✅ **No significant performance changes**\n\n"
    
    # Summary table
    comment += "| Category | Count |\n"
    comment += "|----------|-------|\n"
    comment += f"| 🔴 Regressions | {summary['regressions']} |\n"
    comment += f"| 🟢 Improvements | {summary['improvements']} |\n"
    comment += f"| ⚪ Unchanged | {summary['unchanged']} |\n"
    comment += f"| 🆕 New | {summary['new_benchmarks']} |\n"
    comment += f"| ❌ Missing | {summary['missing_benchmarks']} |\n\n"
    
    # Top regressions
    if report["regressions"]:
        comment += "### 🔴 Top Regressions\n\n"
        comment += "| Benchmark | Baseline | Current | Change |\n"
        comment += "|-----------|----------|---------|--------|\n"
        
        for reg in sorted(report["regressions"], key=lambda x: x["relative_change"], reverse=True)[:5]:
            change_pct = reg["relative_change"] * 100
            comment += f"| {reg['name']} | {reg['baseline_time']:.2f} {reg['unit']} | "
            comment += f"{reg['current_time']:.2f} {reg['unit']} | **+{change_pct:.1f}%** |\n"
        comment += "\n"
    
    # Top improvements
    if report["improvements"]:
        comment += "### 🟢 Top Improvements\n\n"
        comment += "| Benchmark | Baseline | Current | Change |\n"
        comment += "|-----------|----------|---------|--------|\n"
        
        for imp in sorted(report["improvements"], key=lambda x: x["relative_change"])[:5]:
            change_pct = abs(imp["relative_change"]) * 100
            comment += f"| {imp['name']} | {imp['baseline_time']:.2f} {imp['unit']} | "
            comment += f"{imp['current_time']:.2f} {imp['unit']} | **-{change_pct:.1f}%** |\n"
        comment += "\n"
    
    return comment


def main():
    parser = argparse.ArgumentParser(description="Analyze benchmark results for performance regressions")
    parser.add_argument("--current", required=True, help="Current benchmark results JSON file")
    parser.add_argument("--baseline", required=True, help="Baseline benchmark results JSON file")
    parser.add_argument("--threshold", type=float, default=0.10, 
                       help="Regression threshold as decimal (default: 0.10 = 10%)")
    parser.add_argument("--report", help="Output detailed report to JSON file")
    parser.add_argument("--github-comment", help="Output GitHub comment to file")
    parser.add_argument("--fail-on-regression", action="store_true",
                       help="Exit with non-zero status if regressions are found")
    
    args = parser.parse_args()
    
    # Load benchmark results
    print(f"Loading current results: {args.current}")
    current_results = load_benchmark_results(args.current)
    
    print(f"Loading baseline results: {args.baseline}")
    baseline_results = load_benchmark_results(args.baseline)
    
    if not baseline_results.get("benchmarks"):
        print("⚠️ No baseline benchmarks found. Creating initial baseline.")
        # Copy current results as new baseline
        with open(args.baseline, 'w') as f:
            json.dump(current_results, f, indent=2)
        print("✅ Baseline created from current results.")
        return
    
    # Perform comparison
    report = compare_benchmarks(current_results, baseline_results, args.threshold)
    
    # Print console report
    print_regression_report(report, args.threshold)
    
    # Save detailed report
    if args.report:
        import os
        os.makedirs(os.path.dirname(args.report), exist_ok=True)
        with open(args.report, 'w') as f:
            json.dump(report, f, indent=2)
        print(f"📄 Detailed report saved: {args.report}")
    
    # Generate GitHub comment
    if args.github_comment:
        comment = generate_github_comment(report, args.threshold)
        with open(args.github_comment, 'w') as f:
            f.write(comment)
        print(f"💬 GitHub comment saved: {args.github_comment}")
    
    # Exit with error if regressions found and fail-on-regression is set
    if args.fail_on_regression and report["summary"]["regressions"] > 0:
        print(f"\n❌ Failing due to {report['summary']['regressions']} performance regression(s)")
        sys.exit(1)
    
    print("\n✅ Regression analysis completed")


if __name__ == "__main__":
    main()