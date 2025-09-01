
import numpy as np

def rle_encode(arr: np.ndarray):
    """Run-length encode a numpy array with validation."""
    if arr.size == 0:
        return np.array([], dtype=np.uint8), np.array([], dtype=np.int32), arr.shape
        
    flat = arr.ravel()
    diffs = np.r_[True, flat[1:] != flat[:-1]]
    vals = flat[diffs]
    counts = np.diff(np.r_[np.nonzero(diffs)[0], flat.size])
    
    # Validate the encoding
    if np.sum(counts, dtype=np.int64) != flat.size:
        raise ValueError(f"RLE encoding validation failed: sum of counts ({np.sum(counts)}) != array size ({flat.size})")
    
    return vals.astype(np.uint8), counts.astype(np.int32), arr.shape

def rle_decode(vals, counts, shape):
    """Run-length decode with validation."""
    if len(vals) != len(counts):
        raise ValueError(f"RLE decode error: vals length ({len(vals)}) != counts length ({len(counts)})")
    
    if len(vals) == 0:
        return np.zeros(shape, dtype=np.uint8)
        
    expected_size = np.prod(shape)
    actual_size = np.sum(counts, dtype=np.int64)
    
    if actual_size != expected_size:
        raise ValueError(f"RLE decode error: sum of counts ({actual_size}) != expected size ({expected_size})")
    
    flat = np.repeat(vals, counts).astype(np.uint8)
    return flat.reshape(shape)
