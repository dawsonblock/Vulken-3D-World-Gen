# Vulken-3D Production Docker Image
# ==================================
# Multi-stage build for optimized production container

ARG BUILD_TYPE=RelWithDebInfo

# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    python3 \
    python3-pip \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies
RUN pip3 install pyyaml redis

# Setup vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh && \
    ln -s $VCPKG_ROOT/vcpkg /usr/local/bin/vcpkg

# Create build user
RUN useradd -m -u 1000 builder
USER builder
WORKDIR /build

# Copy source code
COPY --chown=builder:builder . /build/

# Configure and build using CI preset
ARG BUILD_TYPE
RUN cmake --preset ci-release \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DVOXELVK_HEADLESS_ONLY=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_METRICS=ON \
    -DENABLE_CRASH_HANDLER=ON

RUN cmake --build build_ci --config ${BUILD_TYPE} -j$(nproc)

# Run tests to ensure build quality
RUN cd build_ci && ctest --output-on-failure -j$(nproc)

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libx11-6 \
    python3 \
    python3-pip \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Python runtime dependencies
RUN pip3 install pyyaml redis

# Create runtime user
RUN useradd -m -u 1001 vulken3d && \
    mkdir -p /app/config /app/assets /app/reports && \
    chown -R vulken3d:vulken3d /app

USER vulken3d
WORKDIR /app

# Copy built binaries and assets
COPY --from=builder --chown=vulken3d:vulken3d /build/build_ci/apps/ /app/bin/
COPY --from=builder --chown=vulken3d:vulken3d /build/config/ /app/config/
COPY --from=builder --chown=vulken3d:vulken3d /build/assets/samples/ /app/assets/
COPY --from=builder --chown=vulken3d:vulken3d /build/scripts/fetch_assets.py /app/scripts/

# Copy shader cache if it exists
COPY --from=builder --chown=vulken3d:vulken3d /build/build_ci/spv/ /app/shaders/ 2>/dev/null || true

# Copy license and documentation
COPY --from=builder --chown=vulken3d:vulken3d /build/LICENSE /app/
COPY --from=builder --chown=vulken3d:vulken3d /build/README.md /app/

# Set up PATH
ENV PATH="/app/bin:$PATH"

# Health check endpoint
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8081/healthz || exit 1

# Default configuration
ENV VULKEN_LOG_LEVEL=INFO
ENV VULKEN_HEADLESS=true
ENV VULKEN_CONFIG_DIR=/app/config
ENV VULKAN_DRIVER=swiftshader

# Expose ports (application and health check)
EXPOSE 8080 8081

# Default command
CMD ["smoke_graphics_headless", "--config", "/app/config/engine.yaml", "--headless"]