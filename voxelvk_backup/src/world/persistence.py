
import json, numpy as np
from pathlib import Path
from typing import Dict, Optional
from concurrent.futures import ThreadPoolExecutor, Future
from .rle import rle_encode, rle_decode

try:
    import zstandard as zstd
    has_zstd=True
except Exception:
    has_zstd=False
try:
    import lz4.frame as lz4f
    has_lz4=True
except Exception:
    has_lz4=False

class ChunkStore:
    def __init__(self, root="world_save", codec="zstd", use_rle=True, threads=4):
        self.root = Path(root); self.root.mkdir(parents=True, exist_ok=True)
        self.codec = codec; self.use_rle = use_rle
        self.pool = ThreadPoolExecutor(max_workers=max(1,threads))
        self.futures: Dict[str, Future] = {}

    def _region_dir(self, cx, cz):
        d = self.root / f"r.{cx//32}.{cz//32}"; d.mkdir(exist_ok=True); return d
    def chunk_path(self, cx, cz): return self._region_dir(cx, cz) / f"c.{cx}.{cz}.bin"

    def _compress(self, b: bytes) -> bytes:
        if self.codec=="zstd" and has_zstd:
            c = zstd.ZstdCompressor(level=10); return c.compress(b)
        if self.codec=="lz4" and has_lz4:
            return lz4f.compress(b, compression_level=9, block_size=lz4f.BLOCKSIZE_MAX1MB, content_checksum=True)
        return b

    def _decompress(self, b: bytes) -> bytes:
        if self.codec=="zstd" and has_zstd:
            d = zstd.ZstdDecompressor(); return d.decompress(b)
        if self.codec=="lz4" and has_lz4:
            return lz4f.decompress(b)
        return b

    def save_chunk_sync(self, chunk):
        """Save a chunk synchronously with proper error handling."""
        try:
            cx, cz = chunk.position
            vox = chunk.voxels.astype(np.uint8)
            if self.use_rle:
                vals, counts, shape = rle_encode(vox)
                header = np.array([shape[0], shape[1], shape[2], vals.size, counts.size], dtype=np.int32).tobytes()
                payload = vals.tobytes() + counts.tobytes()
                data = header + payload
            else:
                header = np.array([vox.shape[0], vox.shape[1], vox.shape[2], 0, 0],dtype=np.int32).tobytes()
                data = header + vox.tobytes()
            blob = self._compress(data)
            
            # Ensure directory exists before writing
            chunk_file = self.chunk_path(cx, cz)
            chunk_file.parent.mkdir(parents=True, exist_ok=True)
            
            chunk_file.write_bytes(blob)
            idx = self._region_dir(cx, cz) / "index.json"
            table = {}
            if idx.exists():
                try: 
                    table = json.loads(idx.read_text())
                except (json.JSONDecodeError, OSError) as e:
                    print(f"Warning: Failed to read index.json for region ({cx//32}, {cz//32}): {e}")
                    table = {}
            table[f"{cx},{cz}"] = {"saved": True}
            idx.write_text(json.dumps(table))
        except Exception as e:
            print(f"Error saving chunk ({cx}, {cz}): {e}")
            raise

    def save_async(self, chunk):
        key = f"{chunk.position[0]},{chunk.position[1]}"
        self.futures[key] = self.pool.submit(self.save_chunk_sync, chunk)

    def load_chunk(self, cx, cz) -> Optional[Dict]:
        """Load a chunk with proper error handling and validation."""
        try:
            p = self.chunk_path(cx, cz)
            if not p.exists(): 
                return None
            blob = p.read_bytes()
            raw = self._decompress(blob)
            
            # Validate header size
            if len(raw) < 20:
                print(f"Warning: Chunk file ({cx}, {cz}) has invalid header size: {len(raw)} bytes")
                return None
                
            header = np.frombuffer(raw[:20], dtype=np.int32)
            H, Y, S, nvals, ncnt = header.tolist()
            
            # Validate header values
            if H <= 0 or Y <= 0 or S <= 0 or nvals < 0 or ncnt < 0:
                print(f"Warning: Chunk file ({cx}, {cz}) has invalid header values: H={H}, Y={Y}, S={S}, nvals={nvals}, ncnt={ncnt}")
                return None
                
            rest = raw[20:]
            if nvals > 0:
                # Fixed: counts are int32 (4 bytes each), so we need nvals + 4*ncnt bytes total
                if len(rest) < nvals + 4*ncnt:
                    print(f"Warning: Chunk file ({cx}, {cz}) has insufficient data for RLE: expected {nvals + 4*ncnt}, got {len(rest)}")
                    return None
                    
                vals = np.frombuffer(rest[:nvals], dtype=np.uint8)
                counts = np.frombuffer(rest[nvals:nvals+4*ncnt], dtype=np.int32)
                
                # Validate counts don't cause overflow
                if np.sum(counts, dtype=np.int64) != H * Y * S:
                    print(f"Warning: Chunk file ({cx}, {cz}) has RLE count mismatch: sum={np.sum(counts)}, expected={H*Y*S}")
                    return None
                    
                vox = rle_decode(vals, counts, (H,Y,S))
            else:
                expected_size = H * Y * S
                if len(rest) < expected_size:
                    print(f"Warning: Chunk file ({cx}, {cz}) has insufficient raw data: expected {expected_size}, got {len(rest)}")
                    return None
                vox = np.frombuffer(rest[:expected_size], dtype=np.uint8).reshape((H,Y,S))
            return {"voxels": vox, "height": int(Y), "size": int(S)}
        except Exception as e:
            print(f"Error loading chunk ({cx}, {cz}): {e}")
            return None

    def wait_all(self):
        for f in list(self.futures.values()):
            try: f.result(timeout=5)
            except Exception: pass
        self.futures.clear()
        self.pool.shutdown(wait=True)
