# Vulken-3D Operator Guide
=========================

## Quick Start

### Local Development Setup

**Prerequisites**
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake ninja-build git curl python3-pip
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Install Vulkan SDK
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list https://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list
sudo apt update && sudo apt install vulkan-sdk

# Python dependencies
pip3 install pyyaml redis
```

**Build and Run**
```bash
git clone https://github.com/your-org/vulken-3d-world-gen.git
cd vulken-3d-world-gen

# Configure and build
cmake --preset linux-default
cmake --build build -j$(nproc)

# Run headless demo
./build/apps/smoke_graphics_headless --config config/engine.yaml

# Run with operator console
./build/apps/operator_console
```

### Docker Deployment

**Single Container**
```bash
# Build image
docker build -t vulken3d .

# Run with Redis
docker run -d --name redis redis:7-alpine
docker run -d --name vulken3d \
  --link redis:redis \
  -p 8080:8080 -p 8081:8081 \
  -e REDIS_HOST=redis \
  vulken3d
```

**Docker Compose (Recommended)**
```bash
# Production deployment
docker-compose up -d

# Development with additional tools
docker-compose --profile dev up -d

# Check status
docker-compose ps
docker-compose logs vulken3d
```

### Kubernetes Deployment

**Prerequisites**
- Kubernetes cluster with GPU node support
- Helm 3.x installed
- NVIDIA device plugin (for GPU acceleration)

**Deploy with Helm**
```bash
# Add GPU node selector (if using GPU)
kubectl label nodes <gpu-node> accelerator=nvidia

# Install Vulken-3D
helm install vulken3d ./helm/vulken-3d \
  --set image.tag=latest \
  --set resources.limits."nvidia\.com/gpu"=1 \
  --set redis.enabled=true

# Check deployment status
kubectl get pods -l app.kubernetes.io/name=vulken-3d
kubectl logs deployment/vulken3d
```

## Operator Console Guide

### Accessing the Console

**Local Access**
- Open browser: `http://localhost:8080`
- Direct executable: `./apps/operator_console`

**Docker Access**
```bash
docker-compose exec vulken3d operator_console
# Or port forward: docker-compose port vulken3d 8080
```

**Kubernetes Access**
```bash
kubectl port-forward service/vulken3d 8080:8080
# Then open: http://localhost:8080
```

### Console Panels Overview

#### 1. World Panel
**Controls**
- **Render Distance**: Adjust visible chunk range (4-16)
- **Regenerate Chunks**: Force regeneration of visible world
- **Clear Cache**: Reset world generation cache

**Monitoring**
- Active chunks currently loaded
- Visible chunks being rendered
- Generation progress indicator

#### 2. Renderer Panel  
**Controls**
- **MSAA Samples**: Anti-aliasing quality (1-8x)
- **Post-Processing**: Toggle SSAO, SSR, Bloom effects
- **Reload Shaders**: Hot-reload shader modifications
- **Clear Pipeline Cache**: Reset compiled shader cache

**Statistics**
- Draw calls per frame
- Total vertex count
- GPU utilization and memory usage

#### 3. Weather Panel
**Controls**
- **Weather Type**: Select from 8 weather conditions
- **Dynamic Weather**: Enable automatic weather transitions  
- **Temperature**: Manual temperature control (-20°C to 40°C)
- **Precipitation**: Rain/snow intensity (0-50 mm/h)
- **Time Controls**: Adjust time of day and season

**Presets**
- **Trigger Storm**: Instant thunderstorm
- **Clear Weather**: Reset to clear conditions

#### 4. Performance Panel
**Real-time Metrics**
- Frame time graph (60-second history)
- CPU and GPU usage trends
- Memory consumption tracking
- FPS stability analysis

**Actions**
- **Save Report**: Export performance data to JSON
- View detailed timing breakdowns

#### 5. Assets Panel
**Asset Management**
- Total loaded assets by type
- Cache hit rate monitoring
- Storage backend status (Redis/filesystem)
- Memory usage by asset category

**Actions**
- **Reload Assets**: Refresh asset cache
- **Test Redis**: Verify Redis connectivity
- Asset loading performance statistics

### Configuration Management

#### Hot-Reload Configuration

**Via Console**
1. Open Configuration panel
2. Modify settings in real-time
3. Click "Save" to persist changes
4. Use "Discard" to revert unsaved changes

**Via File System**
```bash
# Edit configuration files
vim config/engine.yaml
vim config/renderer.yaml

# Reload in console or via API
curl -X POST http://localhost:8081/config/reload
```

#### Configuration Export/Import

**Export Current Settings**
```bash
# Via console: Actions → Export Snapshot
# Via API
curl http://localhost:8081/config/export > my_config.yaml
```

**Import Settings**
```bash
# Copy to config directory
cp my_config.yaml config/engine.yaml

# Reload configuration
curl -X POST http://localhost:8081/config/reload
```

## Monitoring and Logging

### Health Checks

**HTTP Endpoints**
```bash
# Basic health check
curl http://localhost:8081/healthz

# Detailed system status
curl http://localhost:8081/status

# Performance metrics
curl http://localhost:8081/metrics
```

**Response Examples**
```json
{
  "status": "healthy",
  "uptime": "00:15:23",
  "version": "0.9.0-prodp1",
  "vulkan_device": "NVIDIA GeForce RTX 3060",
  "memory_usage": "1.2GB",
  "active_chunks": 256,
  "fps": 58.3
}
```

### Log Management

**Log Levels**
- **TRACE**: Detailed debugging information
- **DEBUG**: Development debugging
- **INFO**: General operational messages (default)
- **WARN**: Warning conditions
- **ERROR**: Error conditions

**Configuration**
```yaml
# config/engine.yaml
engine:
  log_level: "INFO"
  log_file: "/app/logs/vulken3d.log"
  log_rotation: true
  max_log_size: "100MB"
```

**Viewing Logs**
```bash
# Docker
docker-compose logs -f vulken3d

# Kubernetes  
kubectl logs -f deployment/vulken3d

# Local
tail -f logs/vulken3d.log
```

### Performance Monitoring

**Built-in Metrics Collection**
- Automatic performance data collection
- Configurable sampling rates
- Export to JSON, CSV, or Prometheus format

**Integration with Monitoring Systems**
```yaml
# config/monitoring.yaml
monitoring:
  prometheus:
    enabled: true
    port: 8082
    
  grafana:
    dashboard_url: "http://grafana:3000/d/vulken3d"
    
  alerts:
    fps_threshold: 30
    memory_threshold: "4GB"
```

## Troubleshooting

### Common Issues

#### Vulkan Driver Problems
**Symptoms**: Engine fails to start, "No Vulkan devices found"

**Solutions**
```bash
# Verify Vulkan installation
vulkaninfo

# Update graphics drivers
# NVIDIA: Download latest drivers from nvidia.com
# AMD: sudo apt install mesa-vulkan-drivers
# Intel: sudo apt install intel-media-va-driver

# Check Vulkan layers
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
```

#### Performance Issues
**Symptoms**: Low FPS, high latency, stuttering

**Diagnostics**
1. Check GPU utilization in Performance panel
2. Monitor memory usage trends
3. Review render distance settings
4. Verify shader compilation cache

**Solutions**
- Reduce render distance (World panel)
- Disable expensive post-processing effects  
- Lower MSAA samples
- Clear shader cache and regenerate

#### Redis Connection Issues
**Symptoms**: "Redis OFFLINE" in console, asset loading failures

**Solutions**
```bash
# Check Redis service
docker-compose ps redis
kubectl get pods -l app=redis

# Test connectivity
redis-cli -h localhost -p 6379 ping

# Reset Redis data
docker-compose exec redis redis-cli FLUSHALL
```

#### Memory Allocation Failures
**Symptoms**: Crashes, "Out of memory" errors

**Diagnostics**
- Check system memory usage
- Monitor GPU VRAM consumption
- Review asset cache settings

**Solutions**
```yaml
# config/engine.yaml
vulkan:
  memory_budget_mb: 2048  # Reduce if needed

world:
  chunk_cache_size: 512   # Reduce chunk cache

assets:
  cache_size_mb: 256      # Reduce asset cache
```

### Debug Tools

#### Vulkan Validation Layers
```bash
# Enable validation (debug builds only)
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

./vulken3d --vulkan-validation
```

#### Graphics Debugging
```bash
# NVIDIA Nsight Graphics
nsight-gfx ./vulken3d

# AMD Radeon GPU Profiler  
RadeonGPUProfiler ./vulken3d

# Intel GPA
gpa-system-analyzer ./vulken3d
```

#### Performance Profiling
```bash
# CPU profiling with perf
perf record -g ./vulken3d
perf report

# Memory profiling with Valgrind
valgrind --tool=massif ./vulken3d --headless

# Custom profiler
./vulken3d --enable-profiler --profile-output=profile.json
```

### Log Analysis

#### Common Error Patterns

**Shader Compilation Failures**
```
ERROR: Shader compilation failed: main.frag
SPIR-V validation error: ...
```
*Solution*: Check shader syntax, update GPU drivers

**Memory Allocation Failures**  
```
ERROR: VkResult(-3): VK_ERROR_OUT_OF_DEVICE_MEMORY
```
*Solution*: Reduce memory budgets, close other GPU applications

**Asset Loading Timeouts**
```
WARN: Asset loading timeout: texture_large.png
```
*Solution*: Check storage backend connectivity, increase timeout

#### Performance Analysis

**Frame Time Spikes**
```bash
# Filter for slow frames
grep "Frame time:" logs/vulken3d.log | awk '$3 > 20' | head -20
```

**Memory Usage Trends**
```bash
# Extract memory usage over time
grep "Memory usage:" logs/vulken3d.log | cut -d' ' -f3,4 > memory_usage.csv
```

**GPU Utilization Analysis**
```bash
# Monitor GPU usage patterns
nvidia-smi --query-gpu=timestamp,utilization.gpu,memory.used --format=csv -l 1 > gpu_usage.csv
```

## Deployment Best Practices

### Production Configuration

**Security Settings**
```yaml
# config/engine.yaml
engine:
  log_level: "WARN"  # Reduce log verbosity
  
vulkan:
  validation_layers: false  # Disable debug features

monitoring:
  debug_endpoints: false    # Disable debug HTTP endpoints
```

**Performance Optimization**
```yaml
# config/renderer.yaml
renderer:
  msaa_samples: 2          # Balance quality/performance
  
post_processing:
  ssao:
    samples: 8             # Reduce SSAO quality
  ssr:
    enabled: false         # Disable expensive effects
```

### Scaling Considerations

**Resource Limits**
```yaml
# Kubernetes deployment
resources:
  limits:
    nvidia.com/gpu: 1
    memory: 4Gi
    cpu: 2000m
  requests:
    memory: 2Gi
    cpu: 500m
```

**Auto-scaling**
```yaml
# HPA configuration
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: vulken3d-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: vulken3d
  minReplicas: 1
  maxReplicas: 5
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
```

### Backup and Recovery

**Configuration Backup**
```bash
# Backup all config files
tar -czf vulken3d-config-$(date +%Y%m%d).tar.gz config/

# Backup Redis data
docker-compose exec redis redis-cli --rdb /data/backup.rdb
```

**Disaster Recovery**
```bash
# Restore configuration
tar -xzf vulken3d-config-backup.tar.gz

# Restore Redis data
docker-compose exec redis redis-cli --rdb < backup.rdb

# Restart services
docker-compose restart
```

## API Reference

### HTTP Control API

**Base URL**: `http://localhost:8081`

#### Health and Status
```bash
GET  /healthz           # Health check
GET  /status            # Detailed status
GET  /metrics           # Prometheus metrics
GET  /version           # Version information
```

#### Configuration Management
```bash
GET  /config            # Get current configuration
POST /config/reload     # Reload from files
POST /config/export     # Export current config
PUT  /config/{section}  # Update configuration section
```

#### Performance and Monitoring
```bash
GET  /perf/current      # Current performance metrics
GET  /perf/history      # Historical performance data
POST /perf/reset        # Reset performance counters
```

#### World Management
```bash
POST /world/regenerate  # Regenerate visible chunks
POST /world/clear_cache # Clear world generation cache
GET  /world/stats       # World statistics
```

#### Weather Control
```bash
GET  /weather/current   # Current weather state
PUT  /weather/state     # Update weather conditions
POST /weather/preset/{type}  # Apply weather preset
```

For detailed API documentation, see the built-in documentation at `/docs` when the service is running.