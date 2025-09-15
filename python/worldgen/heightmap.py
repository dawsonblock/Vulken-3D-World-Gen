import argparse
import numpy as np
from PIL import Image
try:
    from noise import pnoise2
except ImportError:
    pnoise2 = None

def perlin_heightmap(w: int, h: int, scale: float = 100.0, octaves: int = 6, persistence: float = 0.5, lacunarity: float = 2.0, seed: int = 0):
    if scale <= 0:
        scale = 0.001
    rng = np.random.default_rng(seed)
    ox, oy = rng.integers(0, 10_000), rng.integers(0, 10_000)
    data = np.zeros((h, w), dtype=np.float32)

    if pnoise2 is None:
        # Fallback: smooth random fields
        base = rng.random((h, w), dtype=np.float32)
        from scipy.ndimage import gaussian_filter  # optional
        try:
            data = gaussian_filter(base, sigma=scale/50.0)
        except ValueError:  # Catch specific scipy-related exceptions
            data = base
    else:
        for y in range(h):
            for x in range(w):
                nx = (x + ox) / scale
                ny = (y + oy) / scale
                val = pnoise2(nx, ny, octaves=octaves, persistence=persistence, lacunarity=lacunarity, repeatx=10_000, repeaty=10_000, base=seed)
                data[y, x] = val

    # Normalize to [0,1]
    mn, mx = float(data.min()), float(data.max())
    if mx - mn < 1e-8:
        return np.zeros_like(data)
    return (data - mn) / (mx - mn)

def save_png(heightmap: np.ndarray, path: str):
    img = (np.clip(heightmap, 0.0, 1.0) * 255.0).astype(np.uint8)
    Image.fromarray(img, mode="L").save(path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=512)
    ap.add_argument("--height", type=int, default=512)
    ap.add_argument("--scale", type=float, default=150.0)
    ap.add_argument("--octaves", type=int, default=6)
    ap.add_argument("--persistence", type=float, default=0.5)
    ap.add_argument("--lacunarity", type=float, default=2.0)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--out", type=str, default="./heightmap.png")
    ap.add_argument("--save-npy", type=str, default="")
    args = ap.parse_args()

    hm = perlin_heightmap(args.width, args.height, args.scale, args.octaves, args.persistence, args.lacunarity, args.seed)
    save_png(hm, args.out)
    if args.save_npy:
        np.save(args.save_npy, hm)
    print(f"Saved: {args.out}")

if __name__ == "__main__":
    main()
