
import numpy as np
from .capsule import Capsule
from .capsule_voxel_sat import resolve_capsule_world

class PlayerControllerCapsule:
    def __init__(self, world_manager, spawn, step_height=0.5, gravity=28.0, max_speed=11.0, jump_speed=9.5):
        self.world = world_manager
        self.pos = spawn.astype(np.float32)
        self.vel = np.zeros(3, dtype=np.float32)
        self.radius = 0.3
        self.half_h = 0.9
        self.step_height = step_height
        self.g = gravity
        self.max_speed = max_speed
        self.jump_speed = jump_speed
        self.on_ground = False
        self.input = {"f":0,"b":0,"l":0,"r":0,"jump":0,"sprint":0}

    def set_input(self, m): self.input.update(m)

    def _capsule(self): return Capsule(self.pos.copy(), self.half_h, self.radius)

    def update(self, dt, forward, right):
        """Update player physics with bounds checking."""
        # Validate inputs
        if dt <= 0 or dt > 1.0:  # Prevent extreme timesteps
            print(f"Warning: Invalid timestep dt={dt}, clamping to [0.001, 1.0]")
            dt = max(0.001, min(dt, 1.0))
        
        if np.any(np.isnan(forward)) or np.any(np.isnan(right)):
            print("Warning: NaN detected in movement vectors, resetting to zero")
            forward = np.array([0, 0, 0], dtype=np.float32)
            right = np.array([0, 0, 0], dtype=np.float32)
        
        wish = (forward*(self.input["f"]-self.input["b"]) + right*(self.input["r"]-self.input["l"]))
        wish[1] = 0  # No vertical input movement
        n = np.linalg.norm(wish)
        wish = wish/n if n > 1e-6 else wish
        target = self.max_speed * (1.6 if self.input["sprint"] else 1.0)
        hv = self.vel.copy()
        hv[1] = 0
        self.vel += (wish*target - hv) * min(1.0, (50.0 if self.on_ground else 10.0)*dt)

        self.vel[1] -= self.g*dt
        if self.on_ground and self.input["jump"]:
            self.vel[1] = self.jump_speed
            self.on_ground = False

        # Apply velocity with bounds checking
        old_pos = self.pos.copy()
        self.pos += self.vel*dt
        
        # Prevent extreme positions
        if np.any(np.abs(self.pos) > 1e6):
            print(f"Warning: Player position out of bounds: {self.pos}, resetting to {old_pos}")
            self.pos = old_pos
            self.vel *= 0.1  # Dampen velocity to prevent further issues
        
        cap = self._capsule()
        off, ground = resolve_capsule_world(cap, self.world)
        
        # Step-up logic for stairs
        if np.allclose(off, 0.0, atol=1e-6) and (self.input["f"] or self.input["l"] or self.input["r"] or self.input["b"]):
            cap.center[1] += self.step_height
            off2, ground2 = resolve_capsule_world(cap, self.world)
            if not np.allclose(off2, 0.0, atol=1e-6):
                ground = ground or ground2
        
        self.pos = cap.center
        self.on_ground = ground
        if ground and self.vel[1] < 0: 
            self.vel[1] = 0.0
