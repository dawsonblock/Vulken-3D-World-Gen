#!/usr/bin/env python3
import os, sys, json
from pathlib import Path
from PIL import Image, ImageChops
import numpy as np

golden_dir = Path(os.environ.get("GOLDEN_DIR","tests/golden"))
actual_dir = Path(os.environ.get("ACTUAL_DIR","output/renders"))
out_dir = Path(os.environ.get("DIFF_OUT_DIR","image_diffs"))
out_dir.mkdir(parents=True, exist_ok=True)

ssim_min = float(os.environ.get("DIFF_SSIM_MIN","0.995"))
psnr_min = float(os.environ.get("DIFF_PSNR_MIN","35.0"))
strict = os.environ.get("STRICT_RENDER_TESTS","false").lower() == "true"

def to_gray(a):
    if a.ndim==2: return a.astype(np.float32)/255.0
    if a.shape[2]==3:
        r,g,b = a[:,:,0], a[:,:,1], a[:,:,2]
        return (0.299*r + 0.587*g + 0.114*b).astype(np.float32)/255.0
    if a.shape[2]==4:
        r,g,b = a[:,:,:3].transpose(2,0,1)
        return (0.299*r + 0.587*g + 0.114*b).astype(np.float32)/255.0
    return a.mean(axis=2).astype(np.float32)/255.0

def ssim(img1, img2, C1=0.01**2, C2=0.03**2):
    x = to_gray(img1)
    y = to_gray(img2)
    mu_x = x.mean(); mu_y = y.mean()
    sigma_x = x.var(); sigma_y = y.var()
    sigma_xy = ((x - mu_x)*(y - mu_y)).mean()
    num = (2*mu_x*mu_y + C1)*(2*sigma_xy + C2)
    den = (mu_x**2 + mu_y**2 + C1)*(sigma_x + sigma_y + C2)
    return float(num/den) if den != 0 else 1.0

def psnr(img1, img2, eps=1e-10):
    a = img1.astype(np.float32); b = img2.astype(np.float32)
    mse = ((a-b)**2).mean()
    if mse <= eps: return 99.0
    return 20*np.log10(255.0) - 10*np.log10(mse)

results = []
fails = 0

for gfile in sorted(golden_dir.glob("*.png")):
    name = gfile.stem
    afile = actual_dir / (name + ".png")
    if not afile.exists():
        results.append({"name": name, "status": "missing_actual"})
        if strict: fails += 1
        continue
    g = np.array(Image.open(gfile).convert("RGB"))
    a = np.array(Image.open(afile).convert("RGB"))
    if g.shape != a.shape:
        results.append({"name": name, "status": "shape_mismatch", "golden_shape": tuple(g.shape), "actual_shape": tuple(a.shape)})
        fails += 1
        continue
    s = ssim(g, a)
    p = psnr(g, a)
    diff = ImageChops.difference(Image.fromarray(g), Image.fromarray(a))
    diff_path = out_dir / f"diff_{name}.png"
    diff.save(diff_path)
    status = "pass" if (s >= ssim_min and p >= psnr_min) else "fail"
    if status == "fail": fails += 1
    results.append({"name": name, "status": status, "ssim": s, "psnr": p, "diff": str(diff_path)})

# Write report
report_md = out_dir / "image_diff_report.md"
with open(report_md, "w") as f:
    f.write("# Image Diff Report\n\n")
    for r in results:
        if r["status"] == "pass":
            f.write(f"- **{r['name']}**: PASS (SSIM={r['ssim']:.4f}, PSNR={r['psnr']:.2f} dB)\n")
        elif r["status"] == "fail":
            f.write(f"- **{r['name']}**: **FAIL** (SSIM={r['ssim']:.4f}, PSNR={r['psnr']:.2f} dB) — see `diff_{r['name']}.png`\n")
        elif r["status"] == "missing_actual":
            f.write(f"- **{r['name']}**: **MISSING ACTUAL** (no `{r['name']}.png` in {actual_dir})\n")
        else:
            f.write(f"- **{r['name']}**: **SHAPE MISMATCH** (golden {r['golden_shape']} vs actual {r['actual_shape']})\n")
    f.write("\nThresholds: SSIM>={:.3f}, PSNR>={:.1f} dB.\n".format(ssim_min, psnr_min))
    f.write("Strict mode: {}.\n".format(strict))

with open(out_dir / "image_diff_results.json", "w") as f:
    json.dump(results, f, indent=2)

print(f"Results written to {report_md}")
sys.exit(1 if fails>0 and strict else 0)
