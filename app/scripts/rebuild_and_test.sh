#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
build_dir="${repo_root}/build-tests"

echo "[INFO] Configuring tests build directory at ${build_dir}"
cmake -S "${repo_root}/app" -B "${build_dir}" -DAI_FILE_SORTER_BUILD_TESTS=ON

echo "[INFO] Building tests"
cmake --build "${build_dir}"

echo "[INFO] Running ctest"
ctest --test-dir "${build_dir}"
