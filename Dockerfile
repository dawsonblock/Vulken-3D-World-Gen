# Vulken-3D Production Docker Image
# ==================================
# Multi-stage build for optimized production container

ARG BUILD_TYPE=RelWithDebInfo

# Build stage
FROM ubuntu:22.04 AS build

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ninja-build \
    cmake \
    git \
    python3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Configure and build using CI preset
RUN cmake --preset ci-linux
RUN cmake --build build/ci-linux -j

# Strip binaries in build stage instead of runtime stage  
RUN find build/ci-linux/apps -type f -executable -exec strip --strip-unneeded {} \;

# Runtime stage (distroless - minimal attack surface)
FROM gcr.io/distroless/cc-debian12

# Non-root user for security
USER 65532:65532
WORKDIR /app

# Copy only stripped executables (no build tools, no compilers)
COPY --from=build --chown=65532:65532 /app/build/ci-linux/apps/ /app/

# Default to headless demo (most compatible for containers)
ENTRYPOINT ["/app/smoke_headless"]