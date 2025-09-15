import argparse
import numpy as np
from PIL import Image
try:
    from noise import pnoise2
except ImportError:
    pnoise2 = None

def _blur_np_separable(img: np.ndarray, k: int = 7, passes: int = 2) -> np.ndarray:
    # Simple separable box blur using NumPy only
    k = max(1, int(k) | 1)  # ensure odd >=1
    kernel = np.ones(k, dtype=np.float32) / float(k)

    out = img.astype(np.float32, copy=True)
    for _ in range(max(1, int(passes))):
        # horizontal
        pad = k // 2
        hp = np.pad(out, ((0, 0), (pad, pad)), mode="edge")
        for y in range(out.shape[0]):
            out[y, :] = np.convolve(hp[y, :], kernel, mode="valid")
        # vertical
        vp = np.pad(out, ((pad, pad), (0, 0)), mode="edge")
        for x in range(out.shape[1]):
            out[:, x] = np.convolve(vp[:, x], kernel, mode="valid")
    return out

def perlin_heightmap(w: int, h: int, scale: float = 100.0, octaves: int = 6, persistence: float = 0.5, lacunarity: float = 2.0, seed: int = 0):
    if scale <= 0:
        scale = 0.001
    rng = np.random.default_rng(seed)
    ox, oy = rng.integers(0, 10_000), rng.integers(0, 10_000)
    data = np.zeros((h, w), dtype=np.float32)

    if pnoise2 is None:
        # Fallback: smooth random fields (no SciPy required)
        base = rng.random((h, w), dtype=np.float32)
        data = _blur_np_separable(base, k=max(3, int(scale // 25) | 1), passes=2)
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
    Image.fromarray(img).save(path)

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
