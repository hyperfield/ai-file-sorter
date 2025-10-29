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

# Determine CUDA setting from first argument (default OFF)
CUDASWITCH="OFF"
if [[ "${1,,}" == "cuda=on" ]]; then
    CUDASWITCH="ON"
fi

# Enter llama.cpp directory and build
cd "$LLAMA_DIR"
rm -rf build
mkdir -p build

# Compile shared libs:
echo "Inside script: CC=$CC, CXX=$CXX"

cmake -S . -B build -DGGML_CUDA=$CUDASWITCH \
      -DGGML_OPENCL=OFF \
      -DGGML_BLAS=ON \
      -DGGML_BLAS_VENDOR=OpenBLAS \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10 \
      -DGGML_NATIVE=OFF \
      -DCMAKE_C_FLAGS="-mavx2 -mfma" \
      -DCMAKE_CXX_FLAGS="-mavx2 -mfma"

cmake --build build --config Release -- -j$(nproc)

VARIANT="cpu"
if [[ "$CUDASWITCH" == "ON" ]]; then
    VARIANT="cuda"
fi

VARIANT_ROOT="$PRECOMPILED_ROOT_DIR/$VARIANT"
VARIANT_BIN="$VARIANT_ROOT/bin"
VARIANT_LIB="$VARIANT_ROOT/lib"

rm -rf "$VARIANT_BIN" "$VARIANT_LIB"
mkdir -p "$VARIANT_BIN" "$VARIANT_LIB"

shopt -s nullglob
for so in build/bin/*.so build/bin/*.so.*; do
    cp -P "$so" "$VARIANT_BIN/"
done
for lib in build/lib/*.a; do
    cp "$lib" "$VARIANT_LIB/"
done
for so in build/lib/*.so build/lib/*.so.*; do
    cp -P "$so" "$VARIANT_LIB/"
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
