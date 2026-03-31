#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
default_jobs="$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)"

export AIFS_PLUGIN_BUILD_PROGRAM_NAME="$(basename "$0")"

exec bash "${script_dir}/build_plugins_common.sh" \
    --platform=linux \
    --default-build-dir="${repo_root}/build-plugins-linux" \
    --default-jobs="${default_jobs}" \
    "$@"
