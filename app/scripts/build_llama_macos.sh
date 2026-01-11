#!/bin/bash
set -e

# Resolve script directory (cross-shell portable)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLAMA_DIR="$SCRIPT_DIR/../include/external/llama.cpp"

if [ ! -d "$LLAMA_DIR" ]; then
    echo "Missing llama.cpp submodule. Please run:"
    echo "  git submodule update --init --recursive"
    exit 1
fi

PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../lib/precompiled"
HEADERS_DIR="$SCRIPT_DIR/../include/llama"

ARCH=$(uname -m)
echo "Building on architecture: $ARCH"

MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-11.0}
export MACOSX_DEPLOYMENT_TARGET
echo "Targeting macOS ${MACOSX_DEPLOYMENT_TARGET} for build outputs"

# Decide whether to enable Metal. Apple Silicon machines benefit from it, but
# most Intel Macs either lack usable Metal compute queues or expose 0 bytes of
# GPU memory to ggml, which causes llama.cpp to fail the moment it tries to
# initialize the backend. On Intel default to a CPU-only build; allow overrides
# via LLAMA_MACOS_ENABLE_METAL=1.
ENABLE_METAL=${LLAMA_MACOS_ENABLE_METAL:-auto}
if [ "$ENABLE_METAL" = "auto" ]; then
    if [ "$ARCH" = "arm64" ]; then
        ENABLE_METAL=1
    else
        ENABLE_METAL=0
    fi
fi
if [ "$ENABLE_METAL" != "0" ]; then
    echo "Enabling Metal backend"
    METAL_FLAG=ON
else
    echo "Disabling Metal backend (CPU-only build)"
    METAL_FLAG=OFF
fi

# Ensure SDK paths and libc++ headers are available (especially on Intel Macs).
if command -v xcrun >/dev/null 2>&1; then
    SDKROOT="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)"
    if [ -n "$SDKROOT" ]; then
        export SDKROOT
        export CFLAGS="${CFLAGS} -isysroot ${SDKROOT}"
        export CXXFLAGS="${CXXFLAGS} -isysroot ${SDKROOT} -stdlib=libc++ -I${SDKROOT}/usr/include/c++/v1"
        export LDFLAGS="${LDFLAGS} -isysroot ${SDKROOT}"
        CMAKE_SYSROOT_ARG="-DCMAKE_OSX_SYSROOT=${SDKROOT}"
    fi
fi

# Enter llama.cpp directory and build
cd "$LLAMA_DIR"
rm -rf build
mkdir -p build
cmake -S . -B build \
  ${CMAKE_SYSROOT_ARG} \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
  -DBUILD_SHARED_LIBS=ON \
  -DGGML_METAL=${METAL_FLAG} \
  -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=Accelerate \
  -DGGML_CUDA=OFF \
  -DGGML_OPENCL=OFF \
  -DGGML_VULKAN=OFF \
  -DGGML_SYCL=OFF \
  -DGGML_HIP=OFF \
  -DGGML_KLEIDIAI=OFF \
  -DBLAS_LIBRARIES="-framework Accelerate"

cmake --build build --config Release -- -j$(sysctl -n hw.logicalcpu)

# Copy the resulting dynamic (.dylib) libraries
rm -rf "$PRECOMPILED_LIBS_DIR"
mkdir -p "$PRECOMPILED_LIBS_DIR"
cp build/bin/libllama.dylib "$PRECOMPILED_LIBS_DIR"
cp build/bin/libggml*.dylib "$PRECOMPILED_LIBS_DIR"
cp build/bin/libmtmd.dylib "$PRECOMPILED_LIBS_DIR"
# Provide versioned symlinks expected by the app runtime loader
(
  cd "$PRECOMPILED_LIBS_DIR"
  ln -sf libllama.dylib libllama.0.dylib
  ln -sf libmtmd.dylib libmtmd.0.dylib
)

# Copy headers
rm -rf "$HEADERS_DIR"
mkdir -p "$HEADERS_DIR"
cp include/llama.h "$HEADERS_DIR"
cp ggml/src/*.h "$HEADERS_DIR"
cp ggml/include/*.h "$HEADERS_DIR"
