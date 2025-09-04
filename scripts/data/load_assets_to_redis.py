#!/usr/bin/env python3
"""
Vulken-3D Asset Loading to Redis
================================

This script loads seed assets into Redis for the asset storage backend.
Supports textures, meshes, palettes, and other game assets.

Phase 4 of the Production Upgrade Plan.
"""

import os
import sys
import json
import redis
import hashlib
import base64
from pathlib import Path
from typing import Dict, Any, Optional, List
import argparse


class RedisAssetLoader:
    def __init__(self, redis_host: str = "localhost", redis_port: int = 6379, 
                 redis_db: int = 0, key_prefix: str = "vulken3d:"):
        try:
            self.redis_client = redis.Redis(host=redis_host, port=redis_port, 
                                          db=redis_db, decode_responses=False)
            # Test connection
            self.redis_client.ping()
            print(f"✅ Connected to Redis at {redis_host}:{redis_port}")
            self.offline_mode = False
        except redis.ConnectionError:
            print(f"⚠️  Could not connect to Redis at {redis_host}:{redis_port}")
            print("   Running in offline mode - will create seed assets only")
            self.redis_client = None
            self.offline_mode = True
        
        self.key_prefix = key_prefix
        self.loaded_assets = []
        
    def generate_asset_key(self, asset_type: str, asset_name: str) -> str:
        """Generate Redis key for an asset."""
        return f"{self.key_prefix}asset:{asset_type}:{asset_name}"
    
    def generate_meta_key(self, asset_key: str) -> str:
        """Generate metadata key for an asset."""
        return f"{asset_key}:meta"
    
    def calculate_content_hash(self, content: bytes) -> str:
        """Calculate SHA256 hash of asset content."""
        return hashlib.sha256(content).hexdigest()[:16]
    
    def load_texture_asset(self, texture_path: Path) -> bool:
        """Load a texture asset to Redis."""
        if not texture_path.exists():
            print(f"❌ Texture not found: {texture_path}")
            return False
            
        try:
            # Read texture file
            with open(texture_path, 'rb') as f:
                texture_data = f.read()
            
            asset_name = texture_path.stem
            asset_key = self.generate_asset_key("texture", asset_name)
            meta_key = self.generate_meta_key(asset_key)
            
            # Create metadata
            metadata = {
                "name": asset_name,
                "type": "texture",
                "format": texture_path.suffix[1:].upper(),  # .png -> PNG
                "size": len(texture_data),
                "content_hash": self.calculate_content_hash(texture_data),
                "source_file": str(texture_path),
                "dimensions": "unknown",  # Would need image library to detect
                "compression": "none",
                "mipmaps": False
            }
            
            # Store texture data and metadata
            if not self.offline_mode:
                self.redis_client.set(asset_key, texture_data)
                self.redis_client.set(meta_key, json.dumps(metadata))
            else:
                print(f"    [OFFLINE] Would store to Redis: {asset_key}")
            
            self.loaded_assets.append({"type": "texture", "name": asset_name, "size": len(texture_data)})
            print(f"  ✅ Loaded texture: {asset_name} ({len(texture_data)} bytes)")
            return True
            
        except Exception as e:
            print(f"  ❌ Failed to load texture {texture_path}: {e}")
            return False
    
    def load_mesh_asset(self, mesh_path: Path) -> bool:
        """Load a mesh asset to Redis."""
        if not mesh_path.exists():
            print(f"❌ Mesh not found: {mesh_path}")
            return False
            
        try:
            # Read mesh file
            with open(mesh_path, 'r') as f:
                mesh_data = f.read()
            
            asset_name = mesh_path.stem
            asset_key = self.generate_asset_key("mesh", asset_name)
            meta_key = self.generate_meta_key(asset_key)
            
            # Basic OBJ parsing for vertex count (simplified)
            vertex_count = mesh_data.count('\nv ')
            face_count = mesh_data.count('\nf ')
            
            metadata = {
                "name": asset_name,
                "type": "mesh",
                "format": mesh_path.suffix[1:].upper(),
                "size": len(mesh_data.encode()),
                "content_hash": self.calculate_content_hash(mesh_data.encode()),
                "source_file": str(mesh_path),
                "vertex_count": vertex_count,
                "face_count": face_count,
                "has_normals": '\nvn ' in mesh_data,
                "has_texcoords": '\nvt ' in mesh_data
            }
            
            # Store as string for text formats like OBJ
            if not self.offline_mode:
                self.redis_client.set(asset_key, mesh_data.encode())
                self.redis_client.set(meta_key, json.dumps(metadata))
            else:
                print(f"    [OFFLINE] Would store to Redis: {asset_key}")
            
            self.loaded_assets.append({"type": "mesh", "name": asset_name, "vertices": vertex_count})
            print(f"  ✅ Loaded mesh: {asset_name} ({vertex_count} vertices, {face_count} faces)")
            return True
            
        except Exception as e:
            print(f"  ❌ Failed to load mesh {mesh_path}: {e}")
            return False
    
    def load_palette_asset(self, palette_path: Path) -> bool:
        """Load a palette asset to Redis."""
        if not palette_path.exists():
            print(f"❌ Palette not found: {palette_path}")
            return False
            
        try:
            # Read and parse palette file
            with open(palette_path, 'r') as f:
                palette_data = json.load(f)
            
            asset_name = palette_path.stem
            asset_key = self.generate_asset_key("palette", asset_name)
            meta_key = self.generate_meta_key(asset_key)
            
            # Extract palette information
            biome = palette_data.get("biome", "unknown")
            colors = palette_data.get("colors", [])
            
            metadata = {
                "name": asset_name,
                "type": "palette",
                "format": "JSON",
                "size": len(json.dumps(palette_data)),
                "content_hash": self.calculate_content_hash(json.dumps(palette_data).encode()),
                "source_file": str(palette_path),
                "biome": biome,
                "color_count": len(colors),
                "colors": colors
            }
            
            # Store palette data and metadata
            if not self.offline_mode:
                self.redis_client.set(asset_key, json.dumps(palette_data))
                self.redis_client.set(meta_key, json.dumps(metadata))
            else:
                print(f"    [OFFLINE] Would store to Redis: {asset_key}")
            
            self.loaded_assets.append({"type": "palette", "name": asset_name, "biome": biome})
            print(f"  ✅ Loaded palette: {asset_name} (biome: {biome}, {len(colors)} colors)")
            return True
            
        except Exception as e:
            print(f"  ❌ Failed to load palette {palette_path}: {e}")
            return False
    
    def create_seed_assets(self) -> None:
        """Create minimal seed assets if they don't exist."""
        print("🌱 Creating seed assets...")
        
        assets_dir = Path("assets")
        
        # Create seed textures (placeholder data)
        textures_dir = assets_dir / "textures"
        textures_dir.mkdir(parents=True, exist_ok=True)
        
        seed_textures = {
            "grass_albedo.png": b"PNG_PLACEHOLDER_DATA_FOR_GRASS_TEXTURE",
            "stone_albedo.png": b"PNG_PLACEHOLDER_DATA_FOR_STONE_TEXTURE", 
            "water_normal.png": b"PNG_PLACEHOLDER_DATA_FOR_WATER_NORMAL"
        }
        
        for texture_name, texture_data in seed_textures.items():
            texture_path = textures_dir / texture_name
            if not texture_path.exists():
                with open(texture_path, 'wb') as f:
                    f.write(texture_data)
                print(f"  ✅ Created seed texture: {texture_name}")
        
        # Create seed meshes
        meshes_dir = assets_dir / "meshes_library"
        meshes_dir.mkdir(parents=True, exist_ok=True)
        
        # Simple cube OBJ
        cube_obj = """# Vulken3D Seed Cube Mesh
v -1.0 -1.0  1.0
v  1.0 -1.0  1.0
v  1.0  1.0  1.0
v -1.0  1.0  1.0
v -1.0 -1.0 -1.0
v  1.0 -1.0 -1.0
v  1.0  1.0 -1.0
v -1.0  1.0 -1.0

vn  0.0  0.0  1.0
vn  0.0  0.0 -1.0
vn  0.0  1.0  0.0
vn  0.0 -1.0  0.0
vn  1.0  0.0  0.0
vn -1.0  0.0  0.0

f 1//1 2//1 3//1 4//1
f 8//2 7//2 6//2 5//2
f 4//3 3//3 7//3 8//3
f 5//4 6//4 2//4 1//4
f 2//5 6//5 7//5 3//5
f 8//6 5//6 1//6 4//6
"""
        
        cube_path = meshes_dir / "cube.obj"
        if not cube_path.exists():
            with open(cube_path, 'w') as f:
                f.write(cube_obj)
            print(f"  ✅ Created seed mesh: cube.obj")
        
        # Create seed palettes
        palettes_dir = assets_dir / "palettes"
        palettes_dir.mkdir(parents=True, exist_ok=True)
        
        seed_palettes = {
            "forest.json": {
                "biome": "forest",
                "description": "Lush forest biome with greens and browns",
                "colors": ["#228B22", "#8FBC8F", "#654321", "#90EE90", "#006400"],
                "materials": {
                    "grass": "#228B22",
                    "tree_bark": "#654321",
                    "leaves": "#006400",
                    "dirt": "#8B4513"
                }
            },
            "arctic.json": {
                "biome": "arctic",
                "description": "Cold arctic biome with whites and blues",
                "colors": ["#F0F8FF", "#E0E0E0", "#87CEEB", "#B0C4DE", "#4682B4"],
                "materials": {
                    "snow": "#F0F8FF",
                    "ice": "#87CEEB", 
                    "rock": "#696969",
                    "water": "#4682B4"
                }
            },
            "volcanic.json": {
                "biome": "volcanic",
                "description": "Hot volcanic biome with reds and blacks",
                "colors": ["#FF4500", "#DC143C", "#8B0000", "#000000", "#FF6347"],
                "materials": {
                    "lava": "#FF4500",
                    "obsidian": "#000000",
                    "ash": "#696969",
                    "magma": "#DC143C"
                }
            }
        }
        
        for palette_name, palette_data in seed_palettes.items():
            palette_path = palettes_dir / palette_name
            if not palette_path.exists():
                with open(palette_path, 'w') as f:
                    json.dump(palette_data, f, indent=2)
                print(f"  ✅ Created seed palette: {palette_name}")
    
    def load_all_assets(self, assets_dir: str = "assets") -> None:
        """Load all assets from the assets directory."""
        assets_path = Path(assets_dir)
        if not assets_path.exists():
            print(f"⚠️  Assets directory not found: {assets_path}")
            return
        
        print(f"📁 Loading assets from: {assets_path}")
        
        # Load textures
        textures_dir = assets_path / "textures"
        if textures_dir.exists():
            for texture_file in textures_dir.glob("*"):
                if texture_file.suffix.lower() in ['.png', '.jpg', '.jpeg', '.tga', '.bmp']:
                    self.load_texture_asset(texture_file)
        
        # Load meshes
        meshes_dir = assets_path / "meshes_library"
        if meshes_dir.exists():
            for mesh_file in meshes_dir.glob("*"):
                if mesh_file.suffix.lower() in ['.obj', '.gltf', '.fbx']:
                    self.load_mesh_asset(mesh_file)
        
        # Load palettes
        palettes_dir = assets_path / "palettes"
        if palettes_dir.exists():
            for palette_file in palettes_dir.glob("*.json"):
                self.load_palette_asset(palette_file)
    
    def validate_redis_storage(self) -> bool:
        """Validate that assets were stored correctly in Redis."""
        print("\n🔍 Validating Redis storage...")
        
        # Get all vulken3d keys
        keys = self.redis_client.keys(f"{self.key_prefix}*")
        asset_keys = [k.decode() for k in keys if b':meta' not in k]
        meta_keys = [k.decode() for k in keys if b':meta' in k]
        
        print(f"  📊 Found {len(asset_keys)} assets and {len(meta_keys)} metadata entries")
        
        # Validate each asset has metadata
        validation_success = True
        for asset_key in asset_keys:
            meta_key = self.generate_meta_key(asset_key)
            if meta_key not in meta_keys:
                print(f"  ❌ Missing metadata for asset: {asset_key}")
                validation_success = False
            else:
                # Validate metadata is valid JSON
                try:
                    meta_data = self.redis_client.get(meta_key)
                    json.loads(meta_data)
                except (json.JSONDecodeError, TypeError) as e:
                    print(f"  ❌ Invalid metadata for asset {asset_key}: {e}")
                    validation_success = False
        
        return validation_success
    
    def generate_report(self, output_path: str) -> None:
        """Generate asset loading report."""
        report = {
            "redis_connection": {
                "host": self.redis_client.connection_pool.connection_kwargs.get('host'),
                "port": self.redis_client.connection_pool.connection_kwargs.get('port'),
                "db": self.redis_client.connection_pool.connection_kwargs.get('db')
            },
            "loaded_assets": self.loaded_assets,
            "summary": {
                "total_assets": len(self.loaded_assets),
                "textures": len([a for a in self.loaded_assets if a["type"] == "texture"]),
                "meshes": len([a for a in self.loaded_assets if a["type"] == "mesh"]),
                "palettes": len([a for a in self.loaded_assets if a["type"] == "palette"]),
                "total_size_bytes": sum(a.get("size", 0) for a in self.loaded_assets)
            },
            "validation": {
                "storage_valid": self.validate_redis_storage()
            }
        }
        
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\n📊 Asset loading report: {output_path}")
    
    def run(self, create_seed: bool = False, assets_dir: str = "assets") -> bool:
        """Run the complete asset loading process."""
        print("🚀 Starting asset loading to Redis")
        
        if create_seed:
            self.create_seed_assets()
        
        self.load_all_assets(assets_dir)
        
        success = self.validate_redis_storage()
        
        # Generate report
        self.generate_report("reports/assets/redis_load_report.json")
        
        print(f"\n✅ Asset loading complete! Loaded {len(self.loaded_assets)} assets")
        return success


def main():
    parser = argparse.ArgumentParser(description="Load Vulken-3D assets to Redis")
    parser.add_argument("--host", default="localhost", help="Redis host")
    parser.add_argument("--port", type=int, default=6379, help="Redis port") 
    parser.add_argument("--db", type=int, default=0, help="Redis database")
    parser.add_argument("--prefix", default="vulken3d:", help="Redis key prefix")
    parser.add_argument("--assets-dir", default="assets", help="Assets directory")
    parser.add_argument("--seed", action="store_true", help="Create seed assets")
    
    args = parser.parse_args()
    
    loader = RedisAssetLoader(args.host, args.port, args.db, args.prefix)
    success = loader.run(create_seed=args.seed, assets_dir=args.assets_dir)
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()