FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git python3 python3-pip pkg-config \
    libvulkan-dev vulkan-validationlayers-dev vulkan-tools \
    glslang-tools shaderc \
    ca-certificates curl zip && \
    rm -rf /var/lib/apt/lists/*

# vcpkg (pinned)
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} && \
    ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# Pre-install common libs
COPY vcpkg.json vcpkg-configuration.json /workspace/
RUN cd /workspace && ${VCPKG_ROOT}/vcpkg install

# Workdir
WORKDIR /workspace

# Configure defaults (headless by default)
# Build example:
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVOXELVK_HEADLESS_ONLY=ON
#   cmake --build build --config Release -j
