
import numpy as np
from dataclasses import dataclass

@dataclass
class AABB:
    center: np.ndarray  # (x,y,z) float32
    half:   np.ndarray  # (hx,hy,hz) float32

    @property
    def min(self): return self.center - self.half
    @property
    def max(self): return self.center + self.half

    def moved(self, delta: np.ndarray) -> "AABB":
        return AABB(self.center + delta, self.half)

    def overlap_aabb(self, other: "AABB") -> bool:
        return not (
            self.max[0] <= other.min[0] or self.min[0] >= other.max[0] or
            self.max[1] <= other.min[1] or self.min[1] >= other.max[1] or
            self.max[2] <= other.min[2] or self.min[2] >= other.max[2]
        )
