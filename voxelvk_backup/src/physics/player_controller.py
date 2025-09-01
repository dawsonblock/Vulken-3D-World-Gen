
import numpy as np
from .aabb import AABB
from .voxel_solid import is_solid

class PlayerController:
    """
    Capsule-approximated as AABB (0.6 x 1.8) with swept, axis-separated collision against the voxel grid.
    Integrates gravity, jump, step-up, and ground detection.
    """
    def __init__(self, world_manager, spawn=np.array([0.0, 100.0, 0.0], dtype=np.float32)):
        self.world = world_manager
        self.pos = spawn.astype(np.float32)
        self.vel = np.zeros(3, dtype=np.float32)
        self.aabb = AABB(center=self.pos, half=np.array([0.3, 0.9, 0.3], dtype=np.float32))
        self.gravity = 28.0
        self.max_speed = 11.0
        self.accel = 50.0
        self.air_accel = 10.0
        self.friction = 12.0
        self.jump_speed = 9.5
        self.step_height = 0.5
        self.on_ground = False
        self.input = {"f":0,"b":0,"l":0,"r":0,"up":0,"down":0,"jump":0,"sprint":0}

    def set_input(self, keymap: dict):
        self.input.update({k:int(bool(v)) for k,v in keymap.items() if k in self.input})

    def update(self, dt: float, camera_forward: np.ndarray, camera_right: np.ndarray):
        wish = (camera_forward * (self.input["f"]-self.input["b"]) +
                camera_right   * (self.input["r"]-self.input["l"]))
        wish[1] = 0.0
        wl = np.linalg.norm(wish)
        if wl > 1e-6: wish /= wl
        target_speed = self.max_speed * (1.6 if self.input["sprint"] else 1.0)
        accel = self.accel if self.on_ground else self.air_accel
        hv = self.vel.copy(); hv[1]=0
        self.vel += (wish * target_speed - hv) * min(1.0, accel*dt)
        if self.on_ground and wl < 1e-6:
            self.vel[0] *= max(0.0, 1.0 - self.friction*dt)
            self.vel[2] *= max(0.0, 1.0 - self.friction*dt)
        self.vel[1] -= self.gravity*dt
        if self.on_ground and self.input["jump"]:
            self.vel[1] = self.jump_speed; self.on_ground = False
        pos_before = self.pos.copy()
        self._move_and_collide(dt)
        if np.allclose(self.pos, pos_before, atol=1e-5) and (self.input["f"] or self.input["l"] or self.input["r"] or self.input["b"]):
            lifted = self.pos.copy(); lifted[1] += self.step_height
            if self._can_occupy(lifted):
                self.pos = lifted; self._move_and_collide(dt)

    def _move_and_collide(self, dt: float):
        delta = self.vel * dt
        self.pos, hit_x = self._sweep_axis(self.pos, 0, delta[0])
        self.pos, hit_z = self._sweep_axis(self.pos, 2, delta[2])
        self.pos, hit_y = self._sweep_axis(self.pos, 1, delta[1])
        if hit_x: self.vel[0] = 0.0
        if hit_z: self.vel[2] = 0.0
        if hit_y:
            if delta[1] < 0: self.on_ground = True
            if delta[1] > 0 and self.vel[1] > 0: self.vel[1] = 0.0
        else:
            self.on_ground = False

    def _sweep_axis(self, pos, axis, delta):
        step = np.sign(delta); remaining = abs(delta); hit=False
        while remaining > 1e-6:
            advance = min(remaining, 0.1)
            trial = pos.copy(); trial[axis] += step*advance
            if self._can_occupy(trial):
                pos = trial; remaining -= advance
            else:
                hi, lo = advance, 0.0
                for _ in range(8):
                    mid = 0.5*(hi+lo)
                    trial_mid = pos.copy(); trial_mid[axis] += step*mid
                    if self._can_occupy(trial_mid): lo = mid
                    else: hi = mid
                pos[axis] += step*lo; hit=True; break
        return pos, hit

    def _can_occupy(self, center):
        aabb = AABB(center=center, half=self.aabb.half)
        mn = np.floor(aabb.min).astype(int); mx = np.floor(aabb.max).astype(int)
        for y in range(mn[1], mx[1]+1):
            for z in range(mn[2], mx[2]+1):
                for x in range(mn[0], mx[0]+1):
                    bt = self.world.get_block_at_world_position(float(x), float(y), float(z))
                    if is_solid(bt):
                        if self._aabb_voxel_overlap(aabb, x,y,z): return False
        return True

    @staticmethod
    def _aabb_voxel_overlap(aabb: AABB, x:int, y:int, z:int) -> bool:
        if aabb.max[0] <= x or aabb.min[0] >= x+1: return False
        if aabb.max[1] <= y or aabb.min[1] >= y+1: return False
        if aabb.max[2] <= z or aabb.min[2] >= z+1: return False
        return True
