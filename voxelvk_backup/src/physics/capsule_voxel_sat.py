
import numpy as np
from .capsule import Capsule

def closest_point_on_aabb(p, mn, mx):
    return np.minimum(np.maximum(p, mn), mx)

def closest_point_on_segment(p, a, b):
    """Find closest point on line segment with numeric stability."""
    ab = b - a
    ab_dot = np.dot(ab, ab)
    if ab_dot < 1e-12:  # Degenerate segment
        return a
    t = np.dot(p - a, ab) / ab_dot
    return a + np.clip(t, 0.0, 1.0) * ab

def capsule_box_penetration(cap: Capsule, mn, mx):
    """Compute capsule-box penetration with validation."""
    # Validate box bounds
    if np.any(mn >= mx):
        return False, None, 0.0
        
    # Validate capsule
    if cap.radius <= 0:
        return False, None, 0.0
        
    box_center = (mn + mx) * 0.5
    q_seg = closest_point_on_segment(box_center, cap.seg_a, cap.seg_b)
    q_box = closest_point_on_aabb(q_seg, mn, mx)
    v = q_seg - q_box
    dist = np.linalg.norm(v)
    pen = cap.radius - dist
    
    if pen > 0.0:
        n = (v / (dist + 1e-9)) if dist > 1e-8 else np.array([0,1,0], dtype=np.float32)
        # Ensure normal is valid
        if np.any(np.isnan(n)) or np.any(np.isinf(n)):
            n = np.array([0,1,0], dtype=np.float32)
        return True, n, pen
    return False, None, 0.0

def resolve_capsule_world(cap: Capsule, world, max_iters=8):
    """Resolve capsule collision with world, with bounds checking."""
    # Validate capsule
    if np.any(np.isnan(cap.center)) or np.any(np.isinf(cap.center)):
        print(f"Warning: Invalid capsule center: {cap.center}, resetting to origin")
        cap.center = np.array([0, 2, 0], dtype=np.float32)
    
    if cap.radius <= 0 or cap.half_height <= 0:
        print(f"Warning: Invalid capsule dimensions: radius={cap.radius}, half_height={cap.half_height}")
        cap.radius = max(0.1, cap.radius)
        cap.half_height = max(0.1, cap.half_height)
    
    mn = cap.center - np.array([cap.radius, cap.half_height + cap.radius, cap.radius], dtype=np.float32)
    mx = cap.center + np.array([cap.radius, cap.half_height + cap.radius, cap.radius], dtype=np.float32)
    bb_min = np.floor(mn).astype(int)
    bb_max = np.floor(mx).astype(int)

    # Limit search area to prevent infinite loops
    search_limit = 16  # Maximum blocks to check in each direction
    bb_min = np.maximum(bb_min, cap.center.astype(int) - search_limit)
    bb_max = np.minimum(bb_max, cap.center.astype(int) + search_limit)

    total_offset = np.zeros(3, dtype=np.float32)
    ground = False
    
    for iteration in range(max_iters):
        max_pen = 0.0
        hit_n = None
        contact_n = None
        blocks_checked = 0
        
        for y in range(bb_min[1]-1, bb_max[1]+2):
            for z in range(bb_min[2]-1, bb_max[2]+2):
                for x in range(bb_min[0]-1, bb_max[0]+2):
                    blocks_checked += 1
                    if blocks_checked > 1000:  # Prevent runaway collision checks
                        print(f"Warning: Too many collision checks ({blocks_checked}), breaking")
                        break
                        
                    try:
                        bt = world.get_block_at_world_position(float(x), float(y), float(z))
                    except Exception as e:
                        print(f"Warning: Error getting block at ({x}, {y}, {z}): {e}")
                        bt = 0  # Assume air on error
                        
                    if bt == 0:
                        continue
                    mnv = np.array([x, y, z], dtype=np.float32)
                    mxv = mnv + 1.0
                    hit, n, pen = capsule_box_penetration(cap, mnv, mxv)
                    if hit and pen > max_pen:
                        max_pen, hit_n = pen, n
                        contact_n = n
                        
                if blocks_checked > 1000:
                    break
            if blocks_checked > 1000:
                break
                
        if max_pen <= 1e-6 or hit_n is None: 
            break
            
        # Limit penetration resolution to prevent explosions
        max_pen = min(max_pen, 2.0)  # Max 2 units of correction per iteration
        off = hit_n * max_pen
        cap.center += off
        total_offset += off
        
        # Update bounding box for next iteration
        mn = cap.center - np.array([cap.radius, cap.half_height + cap.radius, cap.radius], dtype=np.float32)
        mx = cap.center + np.array([cap.radius, cap.half_height + cap.radius, cap.radius], dtype=np.float32)
        bb_min = np.floor(mn).astype(int)
        bb_max = np.floor(mx).astype(int)
        bb_min = np.maximum(bb_min, cap.center.astype(int) - search_limit)
        bb_max = np.minimum(bb_max, cap.center.astype(int) + search_limit)
        
        if contact_n is not None and contact_n[1] > 0.7: 
            ground = True
            
    return total_offset, ground
