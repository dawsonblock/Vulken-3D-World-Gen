# Vulken-3D Production Docker Image
# ==================================
# Multi-stage build for optimized production container

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config ninja-build git curl ca-certificates \
    python3 python3-pip python3-numpy python3-pil python3-matplotlib \
    libvulkan-dev vulkan-tools vulkan-validationlayers-dev \
    libglfw3-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
EXPOSE 5173

# Default command is overridden by docker-compose services
CMD ["bash", "-lc", "bash scripts/run.sh --auto=2"]