
import sys
import os
# Add the project root to Python path to allow imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import numpy as np
from src.physics.capsule import Capsule
from src.physics.capsule_voxel_sat import resolve_capsule_world

class DummyWorld:
    def get_block_at_world_position(self, x,y,z): return 1 if int(y)<0 else 0  # ground at y=0

def run():
    world=DummyWorld()
    cap=Capsule(center=np.array([0.0,0.2,0.0],dtype=np.float32), half_height=0.9, radius=0.3)
    off, ground = resolve_capsule_world(cap, world)
    assert ground and cap.center[1]>=0.0, (off, cap.center, ground)

if __name__=="__main__":
    run()
