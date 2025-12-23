#!/bin/bash
set -e

# Resolve script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLAMA_DIR="$SCRIPT_DIR/../include/external/llama.cpp"

if [ ! -d "$LLAMA_DIR" ]; then
    echo "Missing llama.cpp submodule. Please run:"
    echo "  git submodule update --init --recursive"
    exit 1
fi

PRECOMPILED_ROOT_DIR="$SCRIPT_DIR/../lib/precompiled"
HEADERS_DIR="$SCRIPT_DIR/../include/llama"

# Parse optional arguments (cuda=on/off, vulkan=on/off, blas=on/off/auto)
CUDASWITCH="OFF"
VULKANSWITCH="OFF"
BLASSWITCH="AUTO"
for arg in "$@"; do
    case "${arg,,}" in
        cuda=on) CUDASWITCH="ON" ;;
        cuda=off) CUDASWITCH="OFF" ;;
        vulkan=on) VULKANSWITCH="ON" ;;
        vulkan=off) VULKANSWITCH="OFF" ;;
        blas=on) BLASSWITCH="ON" ;;
        blas=off) BLASSWITCH="OFF" ;;
        blas=auto) BLASSWITCH="AUTO" ;;
    esac
done

if [[ "$CUDASWITCH" == "ON" && "$VULKANSWITCH" == "ON" ]]; then
    echo "Cannot enable both CUDA and Vulkan simultaneously. Choose one backend."
    exit 1
fi

echo "CUDA support: $CUDASWITCH"
echo "VULKAN support: $VULKANSWITCH"
echo "BLAS support: $BLASSWITCH (auto prefers OpenBLAS for CPU baseline)"

# Resolve OpenBLAS availability when BLAS is set to AUTO
resolve_blas_setting() {
    local requested="$1"
    if [[ "$requested" == "ON" || "$requested" == "OFF" ]]; then
        echo "$requested"
        return 0
    fi
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists openblas; then
        echo "ON"
        return 0
    fi
    for candidate in /usr/lib/libopenblas.so /usr/lib64/libopenblas.so /usr/lib/x86_64-linux-gnu/libopenblas.so /usr/lib/aarch64-linux-gnu/libopenblas.so; do
        if [ -e "$candidate" ]; then
            echo "ON"
            return 0
        fi
    done
    echo "OFF"
}

build_variant() {
    local variant="$1"
    local cuda_flag="$2"
    local vulkan_flag="$3"
    local blas_flag="$4"
    local runtime_subdir="$5"

    local build_dir="$LLAMA_DIR/build-$variant"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"

    echo "Building variant '$variant' (CUDA=$cuda_flag, VULKAN=$vulkan_flag, BLAS=$blas_flag)..."

    cd "$LLAMA_DIR"

    local cmake_args=(
        -DGGML_CUDA="$cuda_flag"
        -DGGML_VULKAN="$vulkan_flag"
        -DGGML_OPENCL=OFF
        -DGGML_BLAS="$blas_flag"
        -DBUILD_SHARED_LIBS=ON
        -DGGML_NATIVE=OFF
        -DCMAKE_C_FLAGS="-mavx2 -mfma"
        -DCMAKE_CXX_FLAGS="-mavx2 -mfma"
        -S .
        -B "$build_dir"
    )

    if [[ "$blas_flag" == "ON" ]]; then
        cmake_args+=( -DGGML_BLAS_VENDOR=OpenBLAS )
    fi
    if [[ "$cuda_flag" == "ON" ]]; then
        cmake_args+=( -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10 )
    fi

    cmake "${cmake_args[@]}"
    cmake --build "$build_dir" --config Release -- -j"$(nproc)"

    local variant_root="$PRECOMPILED_ROOT_DIR/$variant"
    local variant_bin="$variant_root/bin"
    local variant_lib="$variant_root/lib"
    local ggml_runtime_root="$SCRIPT_DIR/../lib/ggml"
    local runtime_dir="$ggml_runtime_root/$runtime_subdir"

    rm -rf "$variant_bin" "$variant_lib" "$runtime_dir"
    mkdir -p "$variant_bin" "$variant_lib" "$runtime_dir"

    shopt -s nullglob
    for so in "$build_dir"/bin/*.so "$build_dir"/bin/*.so.*; do
        cp -P "$so" "$variant_bin/"
        cp -P "$so" "$runtime_dir/"
    done
    for lib in "$build_dir"/lib/*.a; do
        cp "$lib" "$variant_lib/"
    done
    for so in "$build_dir"/lib/*.so "$build_dir"/lib/*.so.*; do
        cp -P "$so" "$variant_lib/"
        cp -P "$so" "$runtime_dir/"
    done
    shopt -u nullglob

    if command -v patchelf >/dev/null 2>&1; then
        if compgen -G "$variant_bin/"'*.so*' >/dev/null; then
            for lib in "$variant_bin"/*.so*; do
                patchelf --set-rpath '$ORIGIN' "$lib" || true
            done
        fi
    else
        echo "Warning: patchelf not found; skipping RUNPATH fix for llama libraries."
    fi

    cd "$SCRIPT_DIR"
}

# Determine BLAS setting (AUTO falls back to OFF if OpenBLAS is missing)
RESOLVED_BLAS="$(resolve_blas_setting "$BLASSWITCH")"
if [[ "$BLASSWITCH" == "ON" && "$RESOLVED_BLAS" == "OFF" ]]; then
    echo "Requested BLAS=ON but OpenBLAS was not found. Install openblas (or set blas=off) and retry." >&2
    exit 1
fi
if [[ "$RESOLVED_BLAS" == "OFF" && "$BLASSWITCH" == "AUTO" ]]; then
    echo "OpenBLAS not detected; building CPU baseline without BLAS. Install openblas and rerun for BLAS acceleration."
fi

# Always build a CPU baseline (OpenBLAS when available)
build_variant "cpu" "OFF" "OFF" "$RESOLVED_BLAS" "wocuda"

# Build requested accelerator variant if applicable
REQUESTED_VARIANT="cpu"
REQUESTED_RUNTIME="wocuda"
if [[ "$CUDASWITCH" == "ON" ]]; then
    REQUESTED_VARIANT="cuda"
    REQUESTED_RUNTIME="wcuda"
elif [[ "$VULKANSWITCH" == "ON" ]]; then
    REQUESTED_VARIANT="vulkan"
    REQUESTED_RUNTIME="wvulkan"
fi

if [[ "$REQUESTED_VARIANT" != "cpu" ]]; then
    build_variant "$REQUESTED_VARIANT" "$CUDASWITCH" "$VULKANSWITCH" "$RESOLVED_BLAS" "$REQUESTED_RUNTIME"
fi

# Copy headers once (from the source tree)
rm -rf "$HEADERS_DIR" && mkdir -p "$HEADERS_DIR"
cp "$LLAMA_DIR/include/llama.h" "$HEADERS_DIR"
cp "$LLAMA_DIR"/ggml/src/*.h "$HEADERS_DIR"
cp "$LLAMA_DIR"/ggml/include/*.h "$HEADERS_DIR"

# Clean up build directories
rm -rf "$LLAMA_DIR"/build-*
