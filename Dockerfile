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

# Runtime stage (distroless or minimal)
FROM gcr.io/distroless/cc-debian12

USER 65532:65532
WORKDIR /app

ENV STRIP=/usr/bin/strip

# Copy and strip built binaries (only executables)
COPY --from=build /usr/bin/strip /usr/bin/strip
COPY --from=build /app/build/ci-linux/apps /tmp/apps
RUN find /tmp/apps -maxdepth 1 -type f -executable -print -exec /usr/bin/strip --strip-unneeded {} \; -exec mv {} /app/ \; && rm -rf /tmp/apps /usr/bin/strip

# Default command
ENTRYPOINT ["/app/gui_fullscreen_demo"]