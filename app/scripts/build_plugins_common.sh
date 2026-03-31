#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
manifest_path="${script_dir}/plugin_build_targets.tsv"
program_name="${AIFS_PLUGIN_BUILD_PROGRAM_NAME:-$(basename "$0")}"

platform=""
default_build_dir=""
default_jobs=""

for arg in "$@"; do
    case "${arg}" in
        --platform=*)
            platform="${arg#*=}"
            ;;
        --default-build-dir=*)
            default_build_dir="${arg#*=}"
            ;;
        --default-jobs=*)
            default_jobs="${arg#*=}"
            ;;
    esac
done

if [[ -z "${platform}" || -z "${default_build_dir}" || -z "${default_jobs}" ]]; then
    echo "[ERROR] ${program_name} was invoked without required internal options." >&2
    exit 1
fi

build_dir="${default_build_dir}"
jobs="${default_jobs}"
requested_plugins=""
interactive=0
list_only=0

plugin_ids=()
plugin_names=()
plugin_targets=()
plugin_outputs=()

usage() {
    cat <<EOF
Usage: ${program_name} [options]

Build optional storage plugin targets for ${platform}.

Options:
  --list                 List plugin IDs available for this OS and exit.
  --plugins=id1,id2      Build only the specified plugin IDs.
  --interactive          Choose plugins interactively. All are selected by default.
  --build-dir=<path>     CMake build directory. Default: ${default_build_dir}
  --jobs=<n>             Parallel build jobs. Default: ${default_jobs}
  -h, --help             Show this help.

Examples:
  ./${program_name}
  ./${program_name} --list
  ./${program_name} --plugins=onedrive_storage_support
  ./${program_name} --interactive
EOF
}

supports_platform() {
    local platforms="$1"
    local current_platform="$2"
    local padded=",$platforms,"
    case "${padded}" in
        *",${current_platform},"*|*",all,"*|*",any,"*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

load_plugins() {
    if [[ ! -f "${manifest_path}" ]]; then
        echo "[ERROR] Missing plugin manifest: ${manifest_path}" >&2
        exit 1
    fi

    while IFS=$'\t' read -r plugin_id plugin_name cmake_target output_stem platforms; do
        [[ -z "${plugin_id}" ]] && continue
        [[ "${plugin_id#\#}" != "${plugin_id}" ]] && continue
        if supports_platform "${platforms}" "${platform}"; then
            plugin_ids+=("${plugin_id}")
            plugin_names+=("${plugin_name}")
            plugin_targets+=("${cmake_target}")
            plugin_outputs+=("${output_stem}")
        fi
    done < "${manifest_path}"

    if [[ ${#plugin_ids[@]} -eq 0 ]]; then
        echo "[ERROR] No plugin build targets are registered for ${platform}." >&2
        exit 1
    fi
}

list_plugins() {
    echo "Available plugin build targets for ${platform}:"
    local i
    for ((i = 0; i < ${#plugin_ids[@]}; ++i)); do
        printf '  %-28s %s (CMake target: %s)\n' \
            "${plugin_ids[$i]}" "${plugin_names[$i]}" "${plugin_targets[$i]}"
    done
}

prompt_for_plugins() {
    list_plugins
    printf '\nEnter comma-separated plugin IDs to build [all]: '
    local answer
    IFS= read -r answer
    requested_plugins="${answer}"
}

interactive_select_with_checklist() {
    local ui_tool="$1"
    local -a ui_args
    local i
    for ((i = 0; i < ${#plugin_ids[@]}; ++i)); do
        ui_args+=("${plugin_ids[$i]}" "${plugin_names[$i]} (${plugin_targets[$i]})" "ON")
    done

    local selection=""
    if [[ "${ui_tool}" == "whiptail" ]]; then
        selection="$(
            whiptail \
                --title "AI File Sorter Plugins (${platform})" \
                --checklist "Select plugins to build" \
                20 100 10 \
                "${ui_args[@]}" \
                3>&1 1>&2 2>&3
        )"
    else
        selection="$(
            dialog \
                --stdout \
                --title "AI File Sorter Plugins (${platform})" \
                --checklist "Select plugins to build" \
                20 100 10 \
                "${ui_args[@]}"
        )"
    fi

    if [[ -z "${selection}" ]]; then
        requested_plugins=""
        return 0
    fi

    selection="${selection//\"/}"
    selection="${selection//$'\n'/,}"
    selection="${selection// /,}"
    while [[ "${selection}" == *",,"* ]]; do
        selection="${selection//,,/,}"
    done
    selection="${selection#,}"
    selection="${selection%,}"
    requested_plugins="${selection}"
}

choose_plugins_interactively() {
    if [[ -t 0 && -t 1 ]]; then
        if command -v whiptail >/dev/null 2>&1; then
            interactive_select_with_checklist "whiptail"
            return 0
        fi
        if command -v dialog >/dev/null 2>&1; then
            interactive_select_with_checklist "dialog"
            return 0
        fi
    fi
    prompt_for_plugins
}

resolve_requested_targets() {
    selected_ids=()
    selected_targets=()

    if [[ -z "${requested_plugins}" ]]; then
        local i
        for ((i = 0; i < ${#plugin_ids[@]}; ++i)); do
            selected_ids+=("${plugin_ids[$i]}")
            selected_targets+=("${plugin_targets[$i]}")
        done
        return 0
    fi

    local normalized="${requested_plugins// /}"
    local IFS=','
    local requested_id
    for requested_id in ${normalized}; do
        [[ -z "${requested_id}" ]] && continue
        local found=0
        local i
        for ((i = 0; i < ${#plugin_ids[@]}; ++i)); do
            if [[ "${plugin_ids[$i]}" == "${requested_id}" ]]; then
                selected_ids+=("${plugin_ids[$i]}")
                selected_targets+=("${plugin_targets[$i]}")
                found=1
                break
            fi
        done
        if [[ ${found} -eq 0 ]]; then
            echo "[ERROR] Unknown plugin id for ${platform}: ${requested_id}" >&2
            exit 1
        fi
    done

    if [[ ${#selected_ids[@]} -eq 0 ]]; then
        echo "[ERROR] No plugins selected." >&2
        exit 1
    fi
}

configure_build() {
    echo "[INFO] Configuring plugin build directory at ${build_dir}"
    cmake -S "${repo_root}/app" -B "${build_dir}" -DAI_FILE_SORTER_BUILD_TESTS=ON
}

build_plugins() {
    echo "[INFO] Building plugin targets: ${selected_targets[*]}"
    cmake --build "${build_dir}" --target "${selected_targets[@]}" -- -j"${jobs}"
}

print_outputs() {
    echo "[INFO] Requested plugin IDs:"
    local i
    for ((i = 0; i < ${#selected_ids[@]}; ++i)); do
        printf '  - %s\n' "${selected_ids[$i]}"
    done

    echo "[INFO] Built output locations:"
    for ((i = 0; i < ${#selected_ids[@]}; ++i)); do
        local stem="${selected_targets[$i]}"
        local matches
        matches="$(find "${build_dir}" -maxdepth 3 -type f \( -name "${stem}" -o -name "${stem}.exe" -o -name "${plugin_outputs[$i]}" -o -name "${plugin_outputs[$i]}.exe" \) | sort || true)"
        if [[ -n "${matches}" ]]; then
            while IFS= read -r line; do
                [[ -n "${line}" ]] && printf '  - %s\n' "${line}"
            done <<< "${matches}"
        else
            printf '  - %s (target %s)\n' "${plugin_outputs[$i]}" "${selected_targets[$i]}"
        fi
    done
}

user_args=()
for arg in "$@"; do
    case "${arg}" in
        --platform=*|--default-build-dir=*|--default-jobs=*)
            ;;
        --list)
            list_only=1
            ;;
        --plugins=*)
            requested_plugins="${arg#*=}"
            ;;
        --interactive)
            interactive=1
            ;;
        --build-dir=*)
            build_dir="${arg#*=}"
            ;;
        --jobs=*)
            jobs="${arg#*=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            user_args+=("${arg}")
            ;;
    esac
done

if [[ ${#user_args[@]} -ne 0 ]]; then
    usage >&2
    echo "[ERROR] Unknown argument(s): ${user_args[*]}" >&2
    exit 1
fi

load_plugins

if [[ ${list_only} -eq 1 ]]; then
    list_plugins
    exit 0
fi

if [[ ${interactive} -eq 1 ]]; then
    choose_plugins_interactively
fi

resolve_requested_targets
configure_build
build_plugins
print_outputs
