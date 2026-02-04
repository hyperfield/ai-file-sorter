#!/bin/bash
set -e

# CLI flags
usage() {
    cat <<'USAGE'
Usage: build_llama_macos.sh [options]

Options:
  --arch <arm64|x86_64>   Target macOS architecture
  --arm64                 Alias for --arch arm64 (Apple Silicon)
  --x86_64                Alias for --arch x86_64 (Intel)
  --intel                 Alias for --arch x86_64
  --m1 | --m2 | --m3      Alias for --arch arm64
  -h | --help             Show this help

Environment overrides:
  LLAMA_MACOS_ARCH        Target architecture (overridden by CLI)
  LLAMA_MACOS_ENABLE_METAL=0|1|auto
  LLAMA_MACOS_MULTI_VARIANT=0|1
USAGE
}

CLI_ARCH=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)
            if [[ -z "${2:-}" ]]; then
                echo "Missing value for --arch" >&2
                usage
                exit 1
            fi
            CLI_ARCH="$2"
            shift 2
            ;;
        --arm64|--apple-silicon|--m1|--m2|--m3)
            CLI_ARCH="arm64"
            shift
            ;;
        --x86_64|--intel)
            CLI_ARCH="x86_64"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -n "$CLI_ARCH" ]]; then
    case "$CLI_ARCH" in
        arm64|x86_64) ;;
        *)
            echo "Unsupported arch: $CLI_ARCH (use arm64 or x86_64)" >&2
            exit 1
            ;;
    esac
    export LLAMA_MACOS_ARCH="$CLI_ARCH"
fi

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
TARGET_ARCH=${LLAMA_MACOS_ARCH:-$ARCH}
echo "Building on architecture: $ARCH (target: $TARGET_ARCH)"
CROSS_COMPILE=0
if [ "$ARCH" != "$TARGET_ARCH" ]; then
    CROSS_COMPILE=1
fi

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
    if [ "$TARGET_ARCH" = "arm64" ]; then
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

MULTI_VARIANT=${LLAMA_MACOS_MULTI_VARIANT:-0}
CPU_VARIANT_ARGS=""
if [ "$MULTI_VARIANT" = "1" ]; then
    echo "Enabling multi-variant CPU backend build"
    CPU_VARIANT_ARGS="-DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON -DGGML_NATIVE=OFF"
elif [ "$CROSS_COMPILE" = "1" ]; then
    echo "Cross-compiling CPU backend; disabling native CPU targeting"
    CPU_VARIANT_ARGS="-DGGML_NATIVE=OFF"
fi

ARCH_CMAKE_ARG=""
if [ -n "$LLAMA_MACOS_ARCH" ]; then
    ARCH_CMAKE_ARG="-DCMAKE_OSX_ARCHITECTURES=${LLAMA_MACOS_ARCH}"
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
LDFLAGS= cmake -S . -B build \
  ${CMAKE_SYSROOT_ARG} \
  ${ARCH_CMAKE_ARG} \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_EXE_LINKER_FLAGS= \
  -DCMAKE_SHARED_LINKER_FLAGS= \
  -DCMAKE_MODULE_LINKER_FLAGS= \
  ${CPU_VARIANT_ARGS} \
  -DGGML_METAL=${METAL_FLAG} \
  -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=Accelerate \
  -DGGML_CUDA=OFF \
  -DGGML_OPENCL=OFF \
  -DGGML_VULKAN=OFF \
  -DGGML_SYCL=OFF \
  -DGGML_HIP=OFF \
  -DGGML_KLEIDIAI=OFF \
  -DBLAS_LIBRARIES="-framework Accelerate"

LDFLAGS= cmake --build build --config Release -- -j$(sysctl -n hw.logicalcpu)

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

if [ "$MULTI_VARIANT" = "1" ]; then
    shopt -s nullglob
    for backend_lib in build/bin/libggml-*.so; do
        cp "$backend_lib" "$PRECOMPILED_LIBS_DIR"
    done
    shopt -u nullglob
    for backend_dylib in "$PRECOMPILED_LIBS_DIR"/libggml-*.dylib; do
        base="${backend_dylib%.dylib}"
        if [ ! -e "${base}.so" ]; then
            ln -sf "$(basename "$backend_dylib")" "${base}.so"
        fi
    done
fi

# Copy headers
rm -rf "$HEADERS_DIR"
mkdir -p "$HEADERS_DIR"
cp include/llama.h "$HEADERS_DIR"
cp ggml/src/*.h "$HEADERS_DIR"
cp ggml/include/*.h "$HEADERS_DIR"
