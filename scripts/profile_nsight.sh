#!/bin/bash
# VoxelRL_All Profiling Script
# Nsight Systems/Compute helpers for performance analysis

set -e

echo "=== VoxelRL_All Profiling ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
PROFILE_TYPE=${PROFILE_TYPE:-systems}  # systems, compute, or both
PROFILE_DURATION=${PROFILE_DURATION:-30}  # seconds
OUTPUT_DIR=${OUTPUT_DIR:-"$PROJECT_ROOT/profiling"}
TARGET_EXECUTABLE=${TARGET_EXECUTABLE:-VoxelRL_Trainer}
PROFILE_GPU=${PROFILE_GPU:-true}
PROFILE_CPU=${PROFILE_CPU:-true}

# Nsight Systems configuration
NSYS_OPTIONS=${NSYS_OPTIONS:-"--trace=cuda,nvtx,osrt --sample=cpu --cpuctxsw=true"}
NSYS_GPU_METRICS=${NSYS_GPU_METRICS:-"--gpu-metrics-device=all"}

# Nsight Compute configuration
NCU_OPTIONS=${NCU_OPTIONS:-"--metrics=all --kernel-regex=.*"}
NCU_REPLAY_MODE=${NCU_REPLAY_MODE:-kernel}

echo "Profile type: $PROFILE_TYPE"
echo "Duration: $PROFILE_DURATION seconds"
echo "Output directory: $OUTPUT_DIR"
echo "Target executable: $TARGET_EXECUTABLE"
echo "Profile GPU: $PROFILE_GPU"
echo "Profile CPU: $PROFILE_CPU"

# Load environment
if [[ -f "$PROJECT_ROOT/setup_env.sh" ]]; then
    source "$PROJECT_ROOT/setup_env.sh"
fi

# Check profiling tools availability
check_profiling_tools() {
    echo "Checking profiling tools..."
    
    HAS_NSYS=false
    HAS_NCU=false
    
    if command -v nsys &> /dev/null; then
        HAS_NSYS=true
        echo "Nsight Systems found: $(nsys --version | head -n1)"
    else
        echo "Warning: Nsight Systems (nsys) not found"
    fi
    
    if command -v ncu &> /dev/null; then
        HAS_NCU=true
        echo "Nsight Compute found: $(ncu --version | head -n1)"
    else
        echo "Warning: Nsight Compute (ncu) not found"
    fi
    
    # Check if we can profile the requested type
    case "$PROFILE_TYPE" in
        systems)
            if [[ "$HAS_NSYS" == false ]]; then
                echo "Error: Nsight Systems required for systems profiling"
                exit 1
            fi
            ;;
        compute)
            if [[ "$HAS_NCU" == false ]]; then
                echo "Error: Nsight Compute required for compute profiling"
                exit 1
            fi
            ;;
        both)
            if [[ "$HAS_NSYS" == false ]] && [[ "$HAS_NCU" == false ]]; then
                echo "Error: At least one profiling tool required"
                exit 1
            fi
            ;;
    esac
}

# Find target executable
find_target_executable() {
    TARGET_PATHS=(
        "$PROJECT_ROOT/build/$TARGET_EXECUTABLE"
        "$PROJECT_ROOT/build_debug/$TARGET_EXECUTABLE"
        "$PROJECT_ROOT/build_release/$TARGET_EXECUTABLE"
        "$PROJECT_ROOT/build_headless/$TARGET_EXECUTABLE"
    )
    
    for path in "${TARGET_PATHS[@]}"; do
        if [[ -x "$path" ]]; then
            TARGET_PATH="$path"
            break
        fi
    done
    
    if [[ -z "$TARGET_PATH" ]]; then
        echo "Error: Target executable '$TARGET_EXECUTABLE' not found"
        echo "Please build the project first with: ./scripts/build.sh"
        exit 1
    fi
    
    echo "Target path: $TARGET_PATH"
}

# Setup output directory
setup_output() {
    echo "Setting up output directory..."
    
    PROFILE_ID=$(date +"%Y%m%d_%H%M%S")
    PROFILE_DIR="$OUTPUT_DIR/$PROFILE_ID"
    mkdir -p "$PROFILE_DIR"
    
    echo "Profile output: $PROFILE_DIR"
}

# Check GPU availability
check_gpu() {
    echo "Checking GPU availability..."
    
    if command -v nvidia-smi &> /dev/null; then
        echo "NVIDIA GPUs:"
        nvidia-smi --query-gpu=index,name,memory.total,compute_cap --format=csv,noheader
        
        # Get compute capability for profiling optimization
        GPU_COMPUTE_CAP=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader,nounits | head -n1)
        echo "Primary GPU compute capability: $GPU_COMPUTE_CAP"
    else
        echo "Warning: nvidia-smi not found"
        PROFILE_GPU=false
    fi
}

# Generate target arguments
generate_target_args() {
    # Default arguments for profiling (shorter runs)
    TARGET_ARGS=(
        --num-envs 64
        --total-timesteps 10000
        --batch-size 1024
        --checkpoint-interval 10000
        --log-level warn
        --output-dir "$PROFILE_DIR/training_output"
    )
    
    # Add config files if they exist
    if [[ -f "$PROJECT_ROOT/config/world.yaml" ]]; then
        TARGET_ARGS+=(--config-world "$PROJECT_ROOT/config/world.yaml")
    fi
    
    if [[ -f "$PROJECT_ROOT/config/training.yaml" ]]; then
        TARGET_ARGS+=(--config-training "$PROJECT_ROOT/config/training.yaml")
    fi
    
    echo "Target arguments: ${TARGET_ARGS[*]}"
}

# Run Nsight Systems profiling
run_nsys_profiling() {
    echo "Running Nsight Systems profiling..."
    
    NSYS_OUTPUT="$PROFILE_DIR/nsys_profile"
    
    # Construct nsys command
    NSYS_CMD=(
        nsys profile
        --output "$NSYS_OUTPUT"
        --force-overwrite
        --duration "$PROFILE_DURATION"
    )
    
    # Add tracing options
    IFS=' ' read -ra OPTS <<< "$NSYS_OPTIONS"
    NSYS_CMD+=("${OPTS[@]}")
    
    # Add GPU metrics if enabled
    if [[ "$PROFILE_GPU" == true ]]; then
        IFS=' ' read -ra GPU_OPTS <<< "$NSYS_GPU_METRICS"
        NSYS_CMD+=("${GPU_OPTS[@]}")
    fi
    
    # Add target executable and arguments
    NSYS_CMD+=("$TARGET_PATH" "${TARGET_ARGS[@]}")
    
    echo "Nsight Systems command:"
    echo "${NSYS_CMD[*]}"
    echo ""
    
    # Run profiling
    "${NSYS_CMD[@]}"
    
    echo "Nsight Systems profiling complete"
    echo "Report file: ${NSYS_OUTPUT}.nsys-rep"
    
    # Generate text report if available
    if [[ -f "${NSYS_OUTPUT}.nsys-rep" ]]; then
        echo "Generating text report..."
        nsys stats --report gputrace,cudaapisum,memop "${NSYS_OUTPUT}.nsys-rep" > "$PROFILE_DIR/nsys_report.txt"
    fi
}

# Run Nsight Compute profiling
run_ncu_profiling() {
    echo "Running Nsight Compute profiling..."
    
    NCU_OUTPUT="$PROFILE_DIR/ncu_profile"
    
    # Construct ncu command
    NCU_CMD=(
        ncu
        --output "$NCU_OUTPUT"
        --force-overwrite
        --replay-mode "$NCU_REPLAY_MODE"
    )
    
    # Add profiling options
    IFS=' ' read -ra OPTS <<< "$NCU_OPTIONS"
    NCU_CMD+=("${OPTS[@]}")
    
    # Add target executable and arguments
    NCU_CMD+=("$TARGET_PATH" "${TARGET_ARGS[@]}")
    
    echo "Nsight Compute command:"
    echo "${NCU_CMD[*]}"
    echo ""
    
    # Run profiling (with timeout)
    timeout "${PROFILE_DURATION}s" "${NCU_CMD[@]}" || true
    
    echo "Nsight Compute profiling complete"
    echo "Report file: ${NCU_OUTPUT}.ncu-rep"
    
    # Generate text report if available
    if [[ -f "${NCU_OUTPUT}.ncu-rep" ]]; then
        echo "Generating text report..."
        ncu --import "${NCU_OUTPUT}.ncu-rep" --page details > "$PROFILE_DIR/ncu_report.txt" || true
    fi
}

# Generate summary report
generate_summary() {
    echo "Generating profiling summary..."
    
    SUMMARY_FILE="$PROFILE_DIR/profile_summary.md"
    
    cat > "$SUMMARY_FILE" << EOF
# VoxelRL_All Profiling Summary

**Date:** $(date)
**Profile ID:** $PROFILE_ID
**Target:** $TARGET_EXECUTABLE
**Duration:** $PROFILE_DURATION seconds
**Profile Type:** $PROFILE_TYPE

## System Information

**GPU:**
$(nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader 2>/dev/null || echo "Not available")

**CPU:**
$(grep "model name" /proc/cpuinfo | head -n1 | cut -d: -f2 | xargs)

**Memory:**
$(free -h | grep "Mem:" | awk '{print $2}')

## Files Generated

EOF
    
    # Add generated files to summary
    if [[ -f "${PROFILE_DIR}/nsys_profile.nsys-rep" ]]; then
        echo "- \`nsys_profile.nsys-rep\` - Nsight Systems timeline data" >> "$SUMMARY_FILE"
        echo "- \`nsys_report.txt\` - Nsight Systems text report" >> "$SUMMARY_FILE"
    fi
    
    if [[ -f "${PROFILE_DIR}/ncu_profile.ncu-rep" ]]; then
        echo "- \`ncu_profile.ncu-rep\` - Nsight Compute kernel analysis" >> "$SUMMARY_FILE"
        echo "- \`ncu_report.txt\` - Nsight Compute text report" >> "$SUMMARY_FILE"
    fi
    
    cat >> "$SUMMARY_FILE" << EOF

## Viewing Results

**Nsight Systems:**
\`\`\`bash
nsight-sys ${PROFILE_DIR}/nsys_profile.nsys-rep
\`\`\`

**Nsight Compute:**
\`\`\`bash
ncu --import ${PROFILE_DIR}/ncu_profile.ncu-rep
\`\`\`

## Command Line Analysis

**GPU kernel summary:**
\`\`\`bash
nsys stats --report gputrace ${PROFILE_DIR}/nsys_profile.nsys-rep
\`\`\`

**CUDA API summary:**
\`\`\`bash
nsys stats --report cudaapisum ${PROFILE_DIR}/nsys_profile.nsys-rep
\`\`\`
EOF
    
    echo "Summary report: $SUMMARY_FILE"
}

# Print usage
print_usage() {
    cat << EOF
Usage: $0 [options]

Options:
  --type TYPE                 Profiling type: systems, compute, or both (default: systems)
  --duration SECONDS          Profile duration in seconds (default: 30)
  --output-dir PATH           Output directory (default: profiling)
  --target EXECUTABLE         Target executable name (default: VoxelRL_Trainer)
  --no-gpu                    Disable GPU profiling
  --no-cpu                    Disable CPU profiling
  --nsys-options "OPTIONS"    Additional Nsight Systems options
  --ncu-options "OPTIONS"     Additional Nsight Compute options
  --help                      Show this help

Examples:
  # Profile for 60 seconds with both tools
  $0 --type both --duration 60

  # Profile only GPU kernels with Nsight Compute
  $0 --type compute --ncu-options "--kernel-name-base-only"

  # Profile systems timeline with custom options
  $0 --type systems --nsys-options "--trace=cuda,nvtx --sample=cpu"

Environment Variables:
  PROFILE_TYPE                Default profiling type
  PROFILE_DURATION            Default duration
  NSYS_OPTIONS                Default Nsight Systems options
  NCU_OPTIONS                 Default Nsight Compute options
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --type)
            PROFILE_TYPE="$2"
            shift 2
            ;;
        --duration)
            PROFILE_DURATION="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --target)
            TARGET_EXECUTABLE="$2"
            shift 2
            ;;
        --no-gpu)
            PROFILE_GPU=false
            shift
            ;;
        --no-cpu)
            PROFILE_CPU=false
            shift
            ;;
        --nsys-options)
            NSYS_OPTIONS="$2"
            shift 2
            ;;
        --ncu-options)
            NCU_OPTIONS="$2"
            shift 2
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Main execution
main() {
    check_profiling_tools
    find_target_executable
    setup_output
    check_gpu
    generate_target_args
    
    case "$PROFILE_TYPE" in
        systems)
            run_nsys_profiling
            ;;
        compute)
            run_ncu_profiling
            ;;
        both)
            run_nsys_profiling
            echo ""
            run_ncu_profiling
            ;;
        *)
            echo "Error: Invalid profile type '$PROFILE_TYPE'"
            echo "Valid types: systems, compute, both"
            exit 1
            ;;
    esac
    
    generate_summary
    
    echo ""
    echo "=== Profiling Complete ==="
    echo "Output directory: $PROFILE_DIR"
    echo "Summary report: $PROFILE_DIR/profile_summary.md"
}

main "$@"