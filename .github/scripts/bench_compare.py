#!/usr/bin/env python3
# Compare bench_stats.json to .bench/baseline.json and exit nonzero on regression beyond tolerance.
import json, os, sys

def load(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except Exception:
        return {}

CUR = load("bench_stats.json")
BASE = load(".bench/baseline.json")

metric = BASE.get("metric", "mean_frametime_ms")
tol = float(os.environ.get("PERF_REGRESSION_TOLERANCE", "0.20"))  # 0.20 = 20%
strict = os.environ.get("STRICT_BASELINE", "false").lower() == "true"

cur = CUR.get(metric)
base = BASE.get(metric)

report_lines = []
report_lines.append(f"# Perf Compare\n")
report_lines.append(f"- Metric: **{metric}** (lower is better)\n")
report_lines.append(f"- Current: **{cur}**\n")
report_lines.append(f"- Baseline: **{base}**\n")
report_lines.append(f"- Tolerance: **{tol*100:.0f}% regression allowed**\n")

status = "PASS"

if cur is None or base is None:
    report_lines.append("\nBaseline or current stat missing; comparison skipped.")
    if strict:
        status = "FAIL"
        report_lines.append("\nSTRICT_BASELINE=true — failing due to missing baseline/stat.")
else:
    # regression ratio computed as (cur - base)/base; positive => worse if metric is frametime ms
    try:
        reg = (cur - base) / base
    except ZeroDivisionError:
        reg = float("inf")
    report_lines.append(f"\n- Regression ratio: **{reg:.3f}**")
    if reg > tol:
        status = "FAIL"
        report_lines.append("\n❌ Regression exceeds tolerance.")
    else:
        report_lines.append("\n✅ Within tolerance.")

report_lines.append(f"\n**Result: {status}**\n")

with open("perf_compare_report.md","w") as f:
    f.write("\n".join(report_lines))

print("\n".join(report_lines))
sys.exit(1 if status == "FAIL" else 0)
