import argparse
import os
from PIL import Image
import numpy as np
import matplotlib.pyplot as plt

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", type=str, required=True, help="Path to heightmap PNG")
    ap.add_argument("--cmap", type=str, default="terrain")
    ap.add_argument("--surface", action="store_true", help="3D surface view")
    ap.add_argument("--hist", action="store_true", help="Show elevation histogram")
    ap.add_argument("--save", type=str, default="", help="Save figure(s) to path instead of showing")
    args = ap.parse_args()

    if not os.path.exists(args.image):
        raise SystemExit(f"Not found: {args.image}")
    img = Image.open(args.image).convert("L")
    arr = np.array(img, dtype=np.float32) / 255.0

    main_fig = None
    hist_fig = None

    if args.surface:
        from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
        h, w = arr.shape
        xs = np.linspace(0, 1, w)
        ys = np.linspace(0, 1, h)
        X, Y = np.meshgrid(xs, ys)
        fig = plt.figure("Heightmap Surface")
        ax = fig.add_subplot(111, projection="3d")
        ax.plot_surface(X, Y, arr, cmap=args.cmap, linewidth=0, antialiased=True)
        ax.set_xlabel("x")
        ax.set_ylabel("y")
        ax.set_zlabel("elevation")
        plt.tight_layout()
        main_fig = fig
    else:
        fig = plt.figure("Heightmap Viewer")
        plt.imshow(arr, cmap=args.cmap)
        plt.colorbar(label="elevation")
        plt.axis("off")
        plt.tight_layout()
        main_fig = fig

    if args.hist:
        hist_fig = plt.figure("Elevation Histogram")
        plt.hist(arr.ravel(), bins=50, range=(0.0, 1.0), color="gray")
        plt.xlabel("elevation")
        plt.ylabel("count")
        plt.tight_layout()

    if args.save:
        base, ext = os.path.splitext(args.save)
        out_main = args.save if ext else f"{args.save}.png"
        main_fig.savefig(out_main, dpi=150)
        if args.hist and hist_fig is not None:
            out_hist = f"{base}_hist{ext or '.png'}"
            hist_fig.savefig(out_hist, dpi=150)
        return

    plt.show()

if __name__ == "__main__":
    main()
