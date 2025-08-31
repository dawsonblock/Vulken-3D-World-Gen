#!/bin/bash
# VoxelRL_All Setup Script
# Install dependencies and fetch LibTorch CUDA

set -e

echo "=== VoxelRL_All Setup Script ==="
echo "Installing dependencies and setting up environment..."

# Check if running on supported OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    OS="windows"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

# Configuration
VULKAN_SDK_VERSION=${VULKAN_SDK_VERSION:-1.3.275}
LIBTORCH_VERSION=${LIBTORCH_VERSION:-2.1.0}
CUDA_VERSION=${CUDA_VERSION:-12.1}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Project root: $PROJECT_ROOT"
echo "OS: $OS"
echo "Vulkan SDK version: $VULKAN_SDK_VERSION"
echo "LibTorch version: $LIBTORCH_VERSION"
echo "CUDA version: $CUDA_VERSION"

# Install system dependencies
install_system_deps() {
    echo "Installing system dependencies..."
    
    if [[ "$OS" == "linux" ]]; then
        # Update package list
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                cmake \
                ninja-build \
                git \
                wget \
                unzip \
                pkg-config \
                libglfw3-dev \
                libxrandr-dev \
                libxinerama-dev \
                libxcursor-dev \
                libxi-dev \
                libvulkan-dev \
                vulkan-tools \
                vulkan-validationlayers-dev \
                spirv-tools \
                libwayland-dev \
                libxkbcommon-dev \
                python3 \
                python3-pip \
                libyaml-cpp-dev \
                libfmt-dev \
                nlohmann-json3-dev
        elif command -v dnf &> /dev/null; then
            sudo dnf install -y \
                gcc-c++ \
                cmake \
                ninja-build \
                git \
                wget \
                unzip \
                pkgconfig \
                glfw-devel \
                vulkan-devel \
                vulkan-tools \
                vulkan-validation-layers-devel \
                spirv-tools \
                wayland-devel \
                libxkbcommon-devel \
                python3 \
                python3-pip \
                yaml-cpp-devel \
                fmt-devel \
                json-devel
        fi
    fi
}

# Install/setup vcpkg
setup_vcpkg() {
    echo "Setting up vcpkg..."
    
    VCPKG_ROOT="$PROJECT_ROOT/vcpkg"
    
    if [[ ! -d "$VCPKG_ROOT" ]]; then
        git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_ROOT"
    fi
    
    cd "$VCPKG_ROOT"
    if [[ "$OS" == "windows" ]]; then
        ./bootstrap-vcpkg.bat
    else
        ./bootstrap-vcpkg.sh
    fi
    
    echo "vcpkg installed at: $VCPKG_ROOT"
    export VCPKG_ROOT
}

# Install Vulkan SDK
install_vulkan_sdk() {
    echo "Installing Vulkan SDK..."
    
    VULKAN_SDK_DIR="$PROJECT_ROOT/vulkan-sdk"
    
    if [[ -d "$VULKAN_SDK_DIR" ]]; then
        echo "Vulkan SDK already exists at $VULKAN_SDK_DIR"
        return
    fi
    
    if [[ "$OS" == "linux" ]]; then
        VULKAN_SDK_FILE="vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.gz"
        VULKAN_SDK_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/${VULKAN_SDK_FILE}"
        
        wget -q "$VULKAN_SDK_URL"
        tar -xzf "$VULKAN_SDK_FILE"
        mv "${VULKAN_SDK_VERSION}" "$VULKAN_SDK_DIR"
        rm "$VULKAN_SDK_FILE"
    fi
    
    echo "Vulkan SDK installed at: $VULKAN_SDK_DIR"
}

# Fetch LibTorch CUDA
fetch_libtorch() {
    echo "Fetching LibTorch CUDA..."
    
    LIBTORCH_DIR="$PROJECT_ROOT/libtorch"
    
    if [[ -d "$LIBTORCH_DIR" ]]; then
        echo "LibTorch already exists at $LIBTORCH_DIR"
        return
    fi
    
    if [[ "$OS" == "linux" ]]; then
        LIBTORCH_FILE="libtorch-cxx11-abi-shared-with-deps-${LIBTORCH_VERSION}%2Bcu${CUDA_VERSION//./}.zip"
        LIBTORCH_URL="https://download.pytorch.org/libtorch/cu${CUDA_VERSION//./}/${LIBTORCH_FILE}"
    fi
    
    echo "Downloading LibTorch from: $LIBTORCH_URL"
    wget -q "$LIBTORCH_URL" -O libtorch.zip
    unzip -q libtorch.zip
    rm libtorch.zip
    
    echo "LibTorch installed at: $LIBTORCH_DIR"
}

# Setup environment file
setup_environment() {
    echo "Setting up environment..."
    
    ENV_FILE="$PROJECT_ROOT/setup_env.sh"
    
    cat > "$ENV_FILE" << EOF
#!/bin/bash
# VoxelRL_All Environment Setup

export VULKAN_SDK="$PROJECT_ROOT/vulkan-sdk/x86_64"
export PATH="\$PATH:\$VULKAN_SDK/bin"
export LD_LIBRARY_PATH="\$LD_LIBRARY_PATH:\$VULKAN_SDK/lib"
export VK_LAYER_PATH="\$VULKAN_SDK/etc/vulkan/explicit_layer.d"

export LIBTORCH_ROOT="$PROJECT_ROOT/libtorch"
export LD_LIBRARY_PATH="\$LD_LIBRARY_PATH:\$LIBTORCH_ROOT/lib"

export VCPKG_ROOT="$PROJECT_ROOT/vcpkg"

echo "VoxelRL_All environment loaded"
echo "Vulkan SDK: \$VULKAN_SDK"
echo "LibTorch: \$LIBTORCH_ROOT"
echo "vcpkg: \$VCPKG_ROOT"
EOF
    
    chmod +x "$ENV_FILE"
    echo "Environment setup file created: $ENV_FILE"
    echo "Run 'source $ENV_FILE' to load the environment"
}

# Main execution
main() {
    cd "$PROJECT_ROOT"
    
    install_system_deps
    setup_vcpkg
    install_vulkan_sdk
    fetch_libtorch
    setup_environment
    
    echo ""
    echo "=== Setup Complete ==="
    echo "To activate the environment, run:"
    echo "  source $PROJECT_ROOT/setup_env.sh"
    echo ""
    echo "Then build with:"
    echo "  ./scripts/build.sh"
}

main "$@"