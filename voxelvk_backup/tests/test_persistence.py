#!/usr/bin/env python3
"""Test script for persistence system fixes."""

import sys
import os
import numpy as np
import tempfile
import shutil

# Add project root to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from src.world.persistence import ChunkStore
from src.world.rle import rle_encode, rle_decode

class MockChunk:
    """Mock chunk for testing."""
    def __init__(self, position, voxels):
        self.position = position
        self.voxels = voxels

def test_rle_encode_decode():
    """Test RLE encoding and decoding with edge cases."""
    print("Testing RLE encode/decode...")
    
    # Test 1: Simple uniform array
    data = np.ones((4, 4, 4), dtype=np.uint8)
    vals, counts, shape = rle_encode(data)
    decoded = rle_decode(vals, counts, shape)
    assert np.array_equal(data, decoded), "RLE failed for uniform array"
    print("✓ Uniform array test passed")
    
    # Test 2: Alternating pattern
    data = np.zeros((2, 2, 2), dtype=np.uint8)
    data[0, 0, 0] = 1
    data[1, 1, 1] = 2
    vals, counts, shape = rle_encode(data)
    decoded = rle_decode(vals, counts, shape)
    assert np.array_equal(data, decoded), "RLE failed for alternating pattern"
    print("✓ Alternating pattern test passed")
    
    # Test 3: Empty array
    data = np.array([], dtype=np.uint8).reshape(0, 0, 0)
    vals, counts, shape = rle_encode(data)
    decoded = rle_decode(vals, counts, shape)
    assert np.array_equal(data, decoded), "RLE failed for empty array"
    print("✓ Empty array test passed")

def test_persistence_system():
    """Test persistence system with error handling."""
    print("Testing persistence system...")
    
    with tempfile.TemporaryDirectory() as temp_dir:
        store = ChunkStore(root=temp_dir, codec="lz4", use_rle=True)
        
        # Test 1: Save and load normal chunk
        voxels = np.random.randint(0, 10, (32, 32, 32), dtype=np.uint8)
        chunk = MockChunk((0, 0), voxels)
        
        # Save chunk
        store.save_chunk_sync(chunk)
        print("✓ Chunk saved successfully")
        
        # Load chunk
        result = store.load_chunk(0, 0)
        assert result is not None, "Failed to load chunk"
        assert np.array_equal(result['voxels'], voxels), "Loaded data doesn't match saved data"
        print("✓ Chunk loaded and verified successfully")
        
        # Test 2: Load non-existent chunk
        result = store.load_chunk(999, 999)
        assert result is None, "Should return None for non-existent chunk"
        print("✓ Non-existent chunk test passed")
        
        # Test 3: Save uniform chunk (better RLE compression)
        uniform_voxels = np.full((16, 16, 16), 5, dtype=np.uint8)
        uniform_chunk = MockChunk((1, 1), uniform_voxels)
        store.save_chunk_sync(uniform_chunk)
        
        result = store.load_chunk(1, 1)
        assert result is not None, "Failed to load uniform chunk"
        assert np.array_equal(result['voxels'], uniform_voxels), "Uniform chunk data mismatch"
        print("✓ Uniform chunk test passed")

def test_physics_bounds():
    """Test physics system bounds checking."""
    print("Testing physics bounds checking...")
    
    from src.physics.capsule import Capsule
    from src.physics.capsule_voxel_sat import resolve_capsule_world
    
    class MockWorld:
        def get_block_at_world_position(self, x, y, z):
            # Ground at y=0, air elsewhere
            return 1 if int(y) < 0 else 0
    
    world = MockWorld()
    
    # Test with valid capsule
    cap = Capsule(center=np.array([0.0, 2.0, 0.0], dtype=np.float32), 
                  half_height=0.9, radius=0.3)
    offset, ground = resolve_capsule_world(cap, world)
    print("✓ Valid capsule collision test passed")
    
    # Test with extreme position (should be handled gracefully)
    cap = Capsule(center=np.array([1e7, 1e7, 1e7], dtype=np.float32), 
                  half_height=0.9, radius=0.3)
    offset, ground = resolve_capsule_world(cap, world)
    print("✓ Extreme position test passed")

def main():
    """Run all tests."""
    print("Running bug fixes and enhancement tests...\n")
    
    try:
        test_rle_encode_decode()
        print()
        
        test_persistence_system()
        print()
        
        test_physics_bounds()
        print()
        
        print("🎉 All tests passed! Bugs have been fixed and enhancements added.")
        
    except Exception as e:
        print(f"❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())