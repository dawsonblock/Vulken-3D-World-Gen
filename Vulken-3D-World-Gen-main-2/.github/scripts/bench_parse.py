#!/usr/bin/env python3
# Parse headless run logs to estimate FPS/frametime distributions and emit PNG histograms + bench_stats.json.
import sys, re, statistics, json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

log_path = sys.argv[1] if len(sys.argv) > 1 else "bench.log"
with open(log_path, "r", errors="ignore") as f:
    txt = f.read()

fps_vals, ms_vals = [], []

for m in re.finditer(r'(?i)fps[:=\s]+([0-9]+(?:\.[0-9]+)?)', txt):
    try: fps_vals.append(float(m.group(1)))
    except: pass

for m in re.finditer(r'(?i)(frametime|frame\s*time)[:=\s]+([0-9]+(?:\.[0-9]+)?)\s*ms', txt):
    try: ms_vals.append(float(m.group(2)))
    except: pass

if not ms_vals and fps_vals:
    ms_vals = [1000.0/x for x in fps_vals if x > 0]

def q95(arr):
    if not arr: return None
    try:
        return statistics.quantiles(arr, n=20)[-1]
    except Exception:
        return max(arr)

stats = {
    "count_fps": len(fps_vals),
    "count_ms": len(ms_vals),
    "mean_fps": (sum(fps_vals)/len(fps_vals)) if fps_vals else None,
    "median_fps": (statistics.median(fps_vals) if fps_vals else None),
    "mean_frametime_ms": (sum(ms_vals)/len(ms_vals)) if ms_vals else None,
    "median_frametime_ms": (statistics.median(ms_vals) if ms_vals else None),
    "p95_frametime_ms": q95(ms_vals)
}

with open("bench_stats.json","w") as f:
    json.dump(stats, f, indent=2)

if ms_vals:
    plt.figure()
    plt.hist(ms_vals, bins=30)
    plt.xlabel("Frametime (ms)")
    plt.ylabel("Count")
    plt.title("Frametime Histogram\nmean={:.2f} ms, p95={:.2f} ms".format(
        stats["mean_frametime_ms"] or 0.0,
        stats["p95_frametime_ms"] or 0.0))
    plt.savefig("perf_frametime_hist.png", dpi=150)
else:
    open("perf_frametime_hist.png","wb").close()

if fps_vals:
    plt.figure()
    plt.hist(fps_vals, bins=30)
    plt.xlabel("FPS")
    plt.ylabel("Count")
    plt.title("FPS Histogram\nmean={:.2f}, p50={:.2f}".format(
        stats["mean_fps"] or 0.0,
        stats["median_fps"] or 0.0))
    plt.savefig("perf_fps_hist.png", dpi=150)
else:
    open("perf_fps_hist.png","wb").close()
