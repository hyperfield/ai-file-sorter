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

# Parse optional arguments (cuda=on/off, vulkan=on/off)
CUDASWITCH="OFF"
VULKANSWITCH="OFF"
BLASSWITCH="OFF"
for arg in "$@"; do
    case "${arg,,}" in
        cuda=on) CUDASWITCH="ON" ;;
        cuda=off) CUDASWITCH="OFF" ;;
        vulkan=on) VULKANSWITCH="ON" ;;
        vulkan=off) VULKANSWITCH="OFF" ;;
        blas=on) BLASSWITCH="ON" ;;
        blas=off) BLASSWITCH="OFF" ;;
    esac
done

if [[ "$CUDASWITCH" == "ON" && "$VULKANSWITCH" == "ON" ]]; then
    echo "Cannot enable both CUDA and Vulkan simultaneously. Choose one backend."
    exit 1
fi

echo "CUDA support: $CUDASWITCH"
echo "VULKAN support: $VULKANSWITCH"
echo "BLAS support: $BLASSWITCH"

# Enter llama.cpp directory and build
cd "$LLAMA_DIR"
rm -rf build
mkdir -p build

# Compile shared libs:
echo "Inside script: CC=$CC, CXX=$CXX"

cmake_args=(
    -DGGML_CUDA=$CUDASWITCH
    -DGGML_VULKAN=$VULKANSWITCH
    -DGGML_OPENCL=OFF
    -DGGML_BLAS=$BLASSWITCH
    -DBUILD_SHARED_LIBS=ON
    -DGGML_NATIVE=OFF
    -DCMAKE_C_FLAGS="-mavx2 -mfma"
    -DCMAKE_CXX_FLAGS="-mavx2 -mfma"
)

if [[ "$BLASSWITCH" == "ON" ]]; then
    cmake_args+=( -DGGML_BLAS_VENDOR=OpenBLAS )
fi

if [[ "$CUDASWITCH" == "ON" ]]; then
    cmake_args+=( -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10 )
fi

cmake -S . -B build "${cmake_args[@]}"
cmake --build build --config Release -- -j$(nproc)

VARIANT="cpu"
if [[ "$CUDASWITCH" == "ON" ]]; then
    VARIANT="cuda"
elif [[ "$VULKANSWITCH" == "ON" ]]; then
    VARIANT="vulkan"
fi

VARIANT_ROOT="$PRECOMPILED_ROOT_DIR/$VARIANT"
VARIANT_BIN="$VARIANT_ROOT/bin"
VARIANT_LIB="$VARIANT_ROOT/lib"
GGML_RUNTIME_ROOT="$SCRIPT_DIR/../lib/ggml"
RUNTIME_SUBDIR="wocuda"
if [[ "$VARIANT" == "cuda" ]]; then
    RUNTIME_SUBDIR="wcuda"
elif [[ "$VARIANT" == "vulkan" ]]; then
    RUNTIME_SUBDIR="wvulkan"
fi
RUNTIME_DIR="$GGML_RUNTIME_ROOT/$RUNTIME_SUBDIR"

rm -rf "$VARIANT_BIN" "$VARIANT_LIB" "$RUNTIME_DIR"
mkdir -p "$VARIANT_BIN" "$VARIANT_LIB" "$RUNTIME_DIR"

shopt -s nullglob
for so in build/bin/*.so build/bin/*.so.*; do
    cp -P "$so" "$VARIANT_BIN/"
    cp -P "$so" "$RUNTIME_DIR/"
done
for lib in build/lib/*.a; do
    cp "$lib" "$VARIANT_LIB/"
done
for so in build/lib/*.so build/lib/*.so.*; do
    cp -P "$so" "$VARIANT_LIB/"
    cp -P "$so" "$RUNTIME_DIR/"
done
shopt -u nullglob

if command -v patchelf >/dev/null 2>&1; then
    for dir in "$VARIANT_BIN"; do
        if compgen -G "$dir/"'*.so*' >/dev/null; then
            for lib in "$dir"/*.so*; do
                patchelf --set-rpath '$ORIGIN' "$lib" || true
            done
        fi
    done
else
    echo "Warning: patchelf not found; skipping RUNPATH fix for llama libraries."
fi

rm -rf "$HEADERS_DIR" && mkdir -p "$HEADERS_DIR"
cp include/llama.h "$HEADERS_DIR"
cp ggml/src/*.h "$HEADERS_DIR"
cp ggml/include/*.h "$HEADERS_DIR"

rm -rf build
