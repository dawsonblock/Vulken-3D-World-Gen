
import numpy as np

def practical_splits(near, far, n_cascades, lambda_=0.65):
    splits = []
    for i in range(1, n_cascades+1):
        idm = i / n_cascades
        log = near * (far/near)**idm
        uni = near + (far-near)*idm
        splits.append(lambda_*log + (1.0-lambda_)*uni)
    return np.array(splits, dtype=np.float32)

def view_frustum_corners(invViewProj):
    corners_ndc = np.array([[ -1,-1, 0,1],[ 1,-1,0,1],[-1,1,0,1],[1,1,0,1],
                            [ -1,-1, 1,1],[ 1,-1,1,1],[-1,1,1,1],[1,1,1,1]], dtype=np.float32)
    corners_ws = (invViewProj @ corners_ndc.T).T
    corners_ws = corners_ws[:,:3] / corners_ws[:,3:4]
    return corners_ws

def ortho_from_points(points, light_dir, margin=10.0):
    up = np.array([0,1,0],dtype=np.float32)
    if abs(np.dot(light_dir, up)) > 0.99: up = np.array([1,0,0],dtype=np.float32)
    z = -light_dir/np.linalg.norm(light_dir); x = np.cross(up, z); x/=np.linalg.norm(x); y = np.cross(z, x)
    V = np.eye(4,dtype=np.float32); V[:3,:3] = np.stack([x,y,z],axis=0); V[:3,3] = 0
    pts_lv = (V @ np.hstack([points, np.ones((points.shape[0],1),np.float32)]).T).T[:,:3]
    mn = pts_lv.min(0)-margin; mx = pts_lv.max(0)+margin
    O = np.eye(4, dtype=np.float32)
    O[0,0] = 2.0/(mx[0]-mn[0]); O[1,1]= 2.0/(mx[1]-mn[1]); O[2,2]=-2.0/(mx[2]-mn[2])
    O[0,3]=-(mx[0]+mn[0])/(mx[0]-mn[0]); O[1,3]=-(mx[1]+mn[1])/(mx[1]-mn[1]); O[2,3]=-(mx[2]+mn[2])/(mx[2]-mn[2])
    return O @ V
