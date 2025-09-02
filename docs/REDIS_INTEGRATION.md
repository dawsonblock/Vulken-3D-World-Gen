# Redis Asset Streaming Integration

## Overview

The VoxelVK engine includes a production-ready Redis integration for asset streaming and hot-reload capabilities. This system allows for:

- **Asset Streaming**: Efficient loading of mesh and voxel data from Redis
- **Hot-Reload**: Real-time asset updates via Redis pub/sub notifications
- **Production Scale**: Connection pooling and robust error handling
- **Multiple Formats**: Support for OBJ meshes, JSON voxel data, and custom MVOX format

## Architecture

### Components

1. **RedisAssetStore (C++)**: Core asset retrieval system in `src/redis_asset_store.h/cpp`
2. **Asset Tools (Python)**: Command-line tools for pushing assets to Redis
3. **Configuration**: YAML-based configuration in `config/redis.yaml`
4. **Docker Integration**: Redis service included in `docker-compose.yml`

### Asset Namespaces

- `mesh:*` - Mesh data (OBJ, PLY, etc.)
- `voxel:*` - Voxel chunk data (JSON, MVOX, etc.)
- `mat:*` - Material definitions
- `meta:*` - Metadata and asset manifests

## Quick Start

### 1. Start Redis Service

```bash
# Using Docker Compose
docker compose up -d redis

# Verify Redis is running
redis-cli ping
```

### 2. Push Sample Assets

```bash
# Push a mesh asset
python3 tools/push_mesh.py my_cube assets/samples/cube.obj

# Push voxel data
python3 tools/push_voxel.py my_chunk assets/samples/test_voxel_chunk.json
```

### 3. Use in C++ Code

```cpp
#include "redis_asset_store.h"

// Initialize with config
RedisAssetStore store("config/redis.yaml");

// Fetch mesh data
MeshBlob mesh;
if (store.fetch_mesh("my_cube", mesh)) {
    // Process mesh.data
}

// Fetch voxel data
VoxelBlob voxels;
if (store.fetch_voxel_chunk("my_chunk", voxels)) {
    // Process voxels.data
}

// Set up hot-reload callback
store.set_reload_callback([](const std::string& asset_id) {
    std::cout << "Asset updated: " << asset_id << std::endl;
    // Trigger asset reload in your system
});
```

## Configuration

Edit `config/redis.yaml` to customize Redis settings:

```yaml
redis:
  uri: "tcp://127.0.0.1:6379"
  pool_size: 8
  connect_timeout_ms: 200
  socket_timeout_ms: 200
  enable_pubsub: true
  channels: ["assets.mesh", "assets.voxel", "assets.material"]
  
namespaces:
  mesh: "mesh:"
  voxel: "voxel:"
  mat: "mat:"
  meta: "meta:"
```

## Asset Tools

### push_mesh.py

Pushes mesh files to Redis and sends hot-reload notifications.

```bash
python3 tools/push_mesh.py <mesh_id> <file_path>

# Example
python3 tools/push_mesh.py castle_wall assets/meshes/castle_wall.obj
```

### push_voxel.py

Pushes voxel chunk files to Redis and sends hot-reload notifications.

```bash
python3 tools/push_voxel.py <chunk_id> <file_path>

# Example
python3 tools/push_voxel.py chunk_0_0_0 assets/voxels/chunk_0_0_0.json
```

## Sample Assets

The `assets/samples/` directory contains example files for testing:

- `cube.obj` - Simple OBJ mesh file
- `test_voxel_chunk.json` - JSON voxel chunk data
- `simple_voxels.mvox` - Custom MVOX format voxel data

## Hot-Reload System

The Redis integration supports real-time asset updates:

1. **Publisher**: Asset tools publish to channels like `assets.mesh` when assets are updated
2. **Subscriber**: The C++ `RedisAssetStore` subscribes to these channels
3. **Callback**: Your application receives notifications via the reload callback
4. **Action**: Your application can then refetch and reload the updated asset

## Validation

Run the comprehensive validation suite to ensure everything is working:

```bash
python3 tools/validate_redis_integration.py
```

This will test:
- Redis connectivity
- Asset pushing/retrieval
- Hot-reload functionality
- Docker integration
- Configuration validation

## Production Considerations

### Performance
- Connection pooling is enabled by default (8 connections)
- Timeouts are set to reasonable defaults (200ms)
- Binary data is efficiently stored and retrieved

### Reliability
- Automatic reconnection on connection loss
- Graceful handling of Redis unavailability
- Comprehensive error logging

### Security
- Configure Redis authentication in production
- Use TLS for Redis connections in production
- Implement asset access controls as needed

### Monitoring
- Monitor Redis memory usage for large assets
- Track pub/sub message latency
- Monitor connection pool utilization

## Troubleshooting

### Redis Connection Issues
```bash
# Check Redis status
redis-cli ping

# Check Docker containers
docker compose ps

# View Redis logs
docker compose logs redis
```

### Asset Not Found
```bash
# List all assets in Redis
redis-cli KEYS "*"

# Check specific asset
redis-cli GET "mesh:my_asset"
```

### Hot-Reload Not Working
```bash
# Test pub/sub manually
redis-cli PSUBSCRIBE "assets.*"

# In another terminal
redis-cli PUBLISH "assets.mesh" "test_message"
```

## Integration Status

✅ **Redis Service**: Running and accessible  
✅ **Asset Tools**: Working correctly  
✅ **C++ Integration**: RedisAssetStore implemented  
✅ **Hot-Reload**: Pub/sub notifications functional  
✅ **Docker Integration**: Redis container configured  
✅ **Sample Assets**: Test files provided  
✅ **Validation Suite**: Comprehensive testing complete  

## Future Enhancements

- Asset compression for large files
- Asset versioning and rollback
- Distributed asset caching
- Asset dependency tracking
- Automatic asset optimization

---

*Last validated: 2025-09-02 06:43:40 UTC*  
*Validation report: `redis_validation_report.json`*