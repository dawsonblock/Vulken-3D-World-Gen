#!/usr/bin/env python3
"""
Vulken-3D Directory Structure Canonicalization
===============================================

This script ensures the required canonical directory structure exists for the
production build system. Creates missing directories and default configuration files.

Phase 2 of the Production Upgrade Plan.
"""

import os
import json
import yaml
from pathlib import Path
from typing import Dict, Any


class StructureEnsurer:
    def __init__(self, repo_root: str):
        self.repo_root = Path(repo_root)
        
        # Define canonical directory structure
        self.REQUIRED_DIRECTORIES = [
            "assets/textures",
            "assets/meshes_library", 
            "assets/palettes",
            "assets/fonts",
            "config",
            "docs",
            "helm/vulken-3d/templates",
            "include",
            "scripts/clean",
            "scripts/ci", 
            "scripts/data",
            "scripts/dev",
            "scripts/shaders",
            "scripts/bench",
            "shaders/core",
            "shaders/post", 
            "shaders/weather",
            "shaders/debug",
            "tests/unit",
            "tests/integration",
            "tests/bench",
            "tools",
            "reports/snapshots",
            "reports/bench", 
            "reports/shaders",
            "reports/cleanup",
            "reports/inventory",
            "reports/plan",
            "reports/helm"
        ]
        
        # Default configuration files to create
        self.DEFAULT_CONFIGS = {
            "config/engine.yaml": self.create_engine_config,
            "config/renderer.yaml": self.create_renderer_config,
            "config/datasets.yaml": self.create_datasets_config,
            "helm/vulken-3d/Chart.yaml": self.create_helm_chart,
            "helm/vulken-3d/values.yaml": self.create_helm_values,
            "helm/vulken-3d/templates/deployment.yaml": self.create_k8s_deployment,
            "helm/vulken-3d/templates/service.yaml": self.create_k8s_service
        }

    def ensure_directories(self) -> None:
        """Create all required directories if they don't exist."""
        print("📁 Ensuring canonical directory structure...")
        
        created_count = 0
        for dir_path in self.REQUIRED_DIRECTORIES:
            full_path = self.repo_root / dir_path
            if not full_path.exists():
                full_path.mkdir(parents=True, exist_ok=True)
                print(f"  ✅ Created: {dir_path}")
                created_count += 1
            else:
                print(f"  ✓ Exists: {dir_path}")
        
        print(f"📊 Created {created_count} new directories")

    def create_engine_config(self) -> Dict[str, Any]:
        """Create default engine configuration."""
        return {
            "engine": {
                "name": "Vulken-3D-World-Gen",
                "version": "0.9.0-prodp1",
                "log_level": "INFO",
                "headless_fallback": True
            },
            "vulkan": {
                "validation_layers": True,
                "device_selection": "auto",
                "memory_budget_mb": 2048,
                "pipeline_cache": True
            },
            "world": {
                "chunk_size": 64,
                "render_distance": 8,
                "generation_threads": 4,
                "async_loading": True
            },
            "performance": {
                "target_fps": 60,
                "vsync": True,
                "dynamic_resolution": False,
                "cpu_budget_ms": 16.6
            }
        }

    def create_renderer_config(self) -> Dict[str, Any]:
        """Create default renderer configuration."""
        return {
            "renderer": {
                "backend": "vulkan",
                "resolution": {"width": 1920, "height": 1080},
                "msaa_samples": 4,
                "depth_format": "D32_SFLOAT"
            },
            "shaders": {
                "validation": True,
                "hot_reload": True,
                "cache_spirv": True,
                "optimization": "performance"
            },
            "post_processing": {
                "taa": {"enabled": True, "feedback": 0.95},
                "ssao": {"enabled": True, "radius": 0.5, "samples": 16},
                "ssr": {"enabled": False, "max_steps": 64},
                "bloom": {"enabled": True, "threshold": 1.0}
            },
            "shadows": {
                "csm": {
                    "enabled": True,
                    "cascade_count": 4,
                    "resolution": 2048,
                    "lambda": 0.5
                },
                "pcss": {"enabled": True, "sample_radius": 0.01}
            },
            "lighting": {
                "ibl": {"enabled": True},
                "clustered_forward": {"enabled": True, "tile_size": 16}
            }
        }

    def create_datasets_config(self) -> Dict[str, Any]:
        """Create datasets configuration for storage backends."""
        return {
            "storage": {
                "mode": "filesystem",  # filesystem | redis | s3
                "cache_size_mb": 512,
                "async_io": True
            },
            "filesystem": {
                "assets_root": "assets/",
                "cache_dir": ".cache/"
            },
            "redis": {
                "host": "localhost",
                "port": 6379,
                "db": 0,
                "key_prefix": "vulken3d:",
                "compression": "lz4"
            },
            "s3": {
                "bucket": "vulken3d-assets",
                "region": "us-west-2",
                "cache_locally": True
            },
            "assets": {
                "textures": {
                    "formats": ["jpg", "png", "exr"],
                    "max_size_mb": 256
                },
                "meshes": {
                    "formats": ["obj", "gltf", "mvox"],
                    "max_vertices": 100000
                },
                "palettes": {
                    "formats": ["json", "yaml"],
                    "biome_types": ["desert", "forest", "arctic", "volcanic"]
                }
            }
        }

    def create_helm_chart(self) -> Dict[str, Any]:
        """Create Helm Chart.yaml."""
        return {
            "apiVersion": "v2",
            "name": "vulken-3d",
            "description": "Vulken 3D World Generation Engine for Kubernetes",
            "type": "application",
            "version": "0.9.0",
            "appVersion": "0.9.0-prodp1",
            "keywords": ["vulkan", "3d", "world-generation", "voxels", "gpu"],
            "maintainers": [
                {
                    "name": "Vulken Team",
                    "email": "team@vulken3d.io"
                }
            ]
        }

    def create_helm_values(self) -> Dict[str, Any]:
        """Create Helm values.yaml."""
        return {
            "replicaCount": 1,
            "image": {
                "repository": "ghcr.io/vulken/vulken-3d",
                "pullPolicy": "IfNotPresent",
                "tag": "latest"
            },
            "service": {
                "type": "ClusterIP",
                "port": 8080,
                "healthPort": 8081
            },
            "resources": {
                "limits": {
                    "nvidia.com/gpu": 1,
                    "cpu": "2000m",
                    "memory": "4Gi"
                },
                "requests": {
                    "cpu": "500m", 
                    "memory": "2Gi"
                }
            },
            "nodeSelector": {
                "feature.node.kubernetes.io/pci-10de.present": "true"  # NVIDIA GPU
            },
            "tolerations": [
                {
                    "key": "nvidia.com/gpu",
                    "operator": "Exists",
                    "effect": "NoSchedule"
                }
            ],
            "redis": {
                "enabled": True,
                "auth": {"enabled": False}
            },
            "postgres": {
                "enabled": False,
                "auth": {
                    "postgresPassword": "vulken3d",
                    "database": "world_persistence"
                }
            },
            "config": {
                "logLevel": "INFO",
                "headlessMode": True,
                "renderDistance": 8,
                "targetFPS": 60
            }
        }

    def create_k8s_deployment(self) -> str:
        """Create Kubernetes deployment template."""
        return '''apiVersion: apps/v1
kind: Deployment
metadata:
  name: {{ include "vulken-3d.fullname" . }}
  labels:
    {{- include "vulken-3d.labels" . | nindent 4 }}
spec:
  replicas: {{ .Values.replicaCount }}
  selector:
    matchLabels:
      {{- include "vulken-3d.selectorLabels" . | nindent 6 }}
  template:
    metadata:
      labels:
        {{- include "vulken-3d.selectorLabels" . | nindent 8 }}
    spec:
      containers:
      - name: {{ .Chart.Name }}
        image: "{{ .Values.image.repository }}:{{ .Values.image.tag }}"
        imagePullPolicy: {{ .Values.image.pullPolicy }}
        ports:
        - name: http
          containerPort: {{ .Values.service.port }}
          protocol: TCP
        - name: health
          containerPort: {{ .Values.service.healthPort }}
          protocol: TCP
        livenessProbe:
          httpGet:
            path: /healthz
            port: health
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /healthz
            port: health
          initialDelaySeconds: 5
          periodSeconds: 5
        resources:
          {{- toYaml .Values.resources | nindent 10 }}
        env:
        - name: VULKEN_LOG_LEVEL
          value: {{ .Values.config.logLevel }}
        - name: VULKEN_HEADLESS
          value: "{{ .Values.config.headlessMode }}"
        - name: RENDER_DISTANCE
          value: "{{ .Values.config.renderDistance }}"
        volumeMounts:
        - name: config
          mountPath: /app/config
        {{- if .Values.redis.enabled }}
        - name: redis-config
          mountPath: /app/config/redis.yaml
          subPath: redis.yaml
        {{- end }}
      volumes:
      - name: config
        configMap:
          name: {{ include "vulken-3d.fullname" . }}-config
      {{- if .Values.redis.enabled }}
      - name: redis-config
        configMap:
          name: {{ include "vulken-3d.fullname" . }}-redis
      {{- end }}
      {{- with .Values.nodeSelector }}
      nodeSelector:
        {{- toYaml . | nindent 8 }}
      {{- end }}
      {{- with .Values.tolerations }}
      tolerations:
        {{- toYaml . | nindent 8 }}
      {{- end }}
'''

    def create_k8s_service(self) -> str:
        """Create Kubernetes service template."""
        return '''apiVersion: v1
kind: Service
metadata:
  name: {{ include "vulken-3d.fullname" . }}
  labels:
    {{- include "vulken-3d.labels" . | nindent 4 }}
spec:
  type: {{ .Values.service.type }}
  ports:
  - port: {{ .Values.service.port }}
    targetPort: http
    protocol: TCP
    name: http
  - port: {{ .Values.service.healthPort }}
    targetPort: health
    protocol: TCP
    name: health
  selector:
    {{- include "vulken-3d.selectorLabels" . | nindent 4 }}
'''

    def create_default_configs(self) -> None:
        """Create default configuration files if they don't exist."""
        print("⚙️  Creating default configuration files...")
        
        created_count = 0
        for config_path, config_generator in self.DEFAULT_CONFIGS.items():
            full_path = self.repo_root / config_path
            
            if not full_path.exists():
                # Ensure parent directory exists
                full_path.parent.mkdir(parents=True, exist_ok=True)
                
                # Generate and write config
                config_data = config_generator()
                
                if config_path.endswith('.yaml'):
                    with open(full_path, 'w') as f:
                        yaml.dump(config_data, f, default_flow_style=False, sort_keys=False)
                elif config_path.endswith('.json'):
                    with open(full_path, 'w') as f:
                        json.dump(config_data, f, indent=2)
                else:  # Template files
                    with open(full_path, 'w') as f:
                        f.write(config_data)
                
                print(f"  ✅ Created: {config_path}")
                created_count += 1
            else:
                print(f"  ✓ Exists: {config_path}")
        
        print(f"📊 Created {created_count} configuration files")

    def create_placeholder_assets(self) -> None:
        """Create placeholder asset files for demonstration."""
        print("🎨 Creating placeholder assets...")
        
        placeholders = [
            ("assets/textures/default_albedo.png", "# Placeholder texture - replace with real PBR textures"),
            ("assets/meshes_library/cube.obj", "# Basic cube mesh - replace with real geometry"),
            ("assets/palettes/desert.json", '{"biome": "desert", "colors": ["#D2691E", "#CD853F", "#DEB887"]}'),
            ("assets/fonts/README.md", "# Font assets directory\nPlace TTF/OTF fonts here for UI rendering.")
        ]
        
        for asset_path, content in placeholders:
            full_path = self.repo_root / asset_path
            if not full_path.exists():
                full_path.parent.mkdir(parents=True, exist_ok=True)
                with open(full_path, 'w') as f:
                    f.write(content)
                print(f"  ✅ Created placeholder: {asset_path}")

    def run(self) -> None:
        """Execute complete structure canonicalization."""
        print("🏗️  Starting directory structure canonicalization")
        print(f"📁 Repository root: {self.repo_root}")
        
        # Phase 1: Ensure directories exist
        self.ensure_directories()
        
        # Phase 2: Create default configurations  
        self.create_default_configs()
        
        # Phase 3: Create placeholder assets
        self.create_placeholder_assets()
        
        print("\n✅ Directory structure canonicalization complete!")
        print("📋 Next steps:")
        print("   - Review generated configs in config/")
        print("   - Replace placeholder assets with real content")
        print("   - Customize Helm chart values for your environment")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Ensure Vulken-3D canonical directory structure")
    parser.add_argument("--repo-root", default=".", help="Repository root directory")
    
    args = parser.parse_args()
    
    # Create ensurer and run
    ensurer = StructureEnsurer(args.repo_root)
    ensurer.run()


if __name__ == "__main__":
    main()