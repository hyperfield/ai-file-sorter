#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
default_output_dir="${repo_root}/plugins/storage"
default_build_dir="${repo_root}/build-tests"
plugin_id="onedrive_storage_support"
plugin_name="OneDrive Storage Support"
plugin_binary_name="aifs_onedrive_storage_plugin"
category="storage"

base_url=""
output_dir="${default_output_dir}"
build_dir="${default_build_dir}"

usage() {
    cat <<EOF
Usage: $(basename "$0") --base-url=<https://host/path/to/plugins> [options]

Generates a local storage plugin publish payload for the OneDrive plugin.

Options:
  --base-url=<url>       Public HTTPS base URL where the plugin root will be hosted.
                         Example: https://downloads.example.com/aifilesorter/plugins
  --output-dir=<path>    Output directory for the generated payload.
                         Default: ${default_output_dir}
  --build-dir=<path>     Build directory containing ${plugin_binary_name}.
                         Default: ${default_build_dir}
  -h, --help             Show this help.
EOF
}

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

normalize_url() {
    local value="$1"
    while [[ "${value}" == */ ]]; do
        value="${value%/}"
    done
    printf '%s\n' "${value}"
}

detect_platform() {
    case "$(uname -s)" in
        Linux) printf 'linux\n' ;;
        Darwin) printf 'macos\n' ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT) printf 'windows\n' ;;
        *)
            fail "Unsupported platform: $(uname -s)"
            ;;
    esac
}

detect_architecture() {
    case "$(uname -m)" in
        x86_64|amd64) printf 'x86_64\n' ;;
        aarch64|arm64) printf 'arm64\n' ;;
        i386|i686) printf 'x86\n' ;;
        armv7l|armv6l|arm) printf 'arm\n' ;;
        *)
            fail "Unsupported architecture: $(uname -m)"
            ;;
    esac
}

extract_plugin_version() {
    local manifest_cpp="${repo_root}/app/lib/StoragePluginManifest.cpp"
    local version
    version="$(
        awk '
            /id = "onedrive_storage_support"/ { in_plugin = 1; next }
            in_plugin && /version = "/ {
                if (match($0, /"[^"]+"/)) {
                    print substr($0, RSTART + 1, RLENGTH - 2)
                    exit
                }
            }
            in_plugin && /StoragePluginManifest\{/ && !/onedrive_storage_support/ { in_plugin = 0 }
        ' "${manifest_cpp}"
    )"
    [[ -n "${version}" ]] || fail "Failed to determine ${plugin_id} version from ${manifest_cpp}"
    printf '%s\n' "${version}"
}

escape_json() {
    local value="$1"
    value="${value//\\/\\\\}"
    value="${value//\"/\\\"}"
    value="${value//$'\n'/\\n}"
    printf '%s' "${value}"
}

for arg in "$@"; do
    case "${arg}" in
        --base-url=*)
            base_url="${arg#*=}"
            ;;
        --output-dir=*)
            output_dir="${arg#*=}"
            ;;
        --build-dir=*)
            build_dir="${arg#*=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            fail "Unknown argument: ${arg}"
            ;;
    esac
done

[[ -n "${base_url}" ]] || {
    usage >&2
    fail "Missing required --base-url argument."
}

base_url="$(normalize_url "${base_url}")"
category_base_url="${base_url}/${category}"
platform="$(detect_platform)"
architecture="$(detect_architecture)"
runtime_id="${platform}-${architecture}"
plugin_version="$(extract_plugin_version)"
binary_path="${build_dir}/${plugin_binary_name}"

[[ -x "${binary_path}" ]] || fail "Plugin binary not found or not executable: ${binary_path}"
command -v zip >/dev/null 2>&1 || fail "The 'zip' command is required."
command -v sha256sum >/dev/null 2>&1 || fail "The 'sha256sum' command is required."

plugin_dir="${output_dir}/onedrive/${runtime_id}"
legacy_plugin_dir="${output_dir}/onedrive"
package_name="${plugin_id}-${plugin_version}-${platform}-${architecture}.aifsplugin"
package_path="${plugin_dir}/${package_name}"
runtime_manifest_path="${plugin_dir}/manifest.json"
catalog_path="${output_dir}/catalog.json"
checksums_path="${output_dir}/SHA256SUMS"
readme_path="${output_dir}/README.md"

mkdir -p "${plugin_dir}"

# Clean up the pre-runtime layout so the payload tree reflects the new per-OS structure.
if [[ -f "${legacy_plugin_dir}/manifest.json" ]]; then
    rm -f "${legacy_plugin_dir}/manifest.json"
fi
find "${legacy_plugin_dir}" -maxdepth 1 -type f -name '*.aifsplugin' -delete 2>/dev/null || true

tmp_dir="$(mktemp -d)"
cleanup() {
    rm -rf "${tmp_dir}"
}
trap cleanup EXIT

mkdir -p "${tmp_dir}/bin"
cp "${binary_path}" "${tmp_dir}/bin/${plugin_binary_name}"
chmod +x "${tmp_dir}/bin/${plugin_binary_name}"

cat > "${tmp_dir}/manifest.json" <<EOF
{
  "id": "$(escape_json "${plugin_id}")",
  "name": "$(escape_json "${plugin_name}")",
  "description": "Adds a dedicated OneDrive connector process with stronger sync-state detection, move preflight checks, and richer undo metadata for synced folders.",
  "version": "$(escape_json "${plugin_version}")",
  "provider_ids": ["onedrive"],
  "platforms": ["$(escape_json "${platform}")"],
  "architectures": ["$(escape_json "${architecture}")"],
  "entry_point_kind": "external_process",
  "entry_point": "bin/${plugin_binary_name}",
  "package_paths": ["bin/${plugin_binary_name}"]
}
EOF

(
    cd "${tmp_dir}"
    zip -qr "${package_path}" manifest.json bin
)

package_sha="$(sha256sum "${package_path}" | awk '{print $1}')"

cat > "${runtime_manifest_path}" <<EOF
{
  "id": "$(escape_json "${plugin_id}")",
  "name": "$(escape_json "${plugin_name}")",
  "description": "Adds a dedicated OneDrive connector process with stronger sync-state detection, move preflight checks, and richer undo metadata for synced folders.",
  "version": "$(escape_json "${plugin_version}")",
  "provider_ids": ["onedrive"],
  "platforms": ["$(escape_json "${platform}")"],
  "architectures": ["$(escape_json "${architecture}")"],
  "remote_manifest_url": "$(escape_json "${category_base_url}/onedrive/${runtime_id}/manifest.json")",
  "package_download_url": "$(escape_json "${category_base_url}/onedrive/${runtime_id}/${package_name}")",
  "package_sha256": "${package_sha}",
  "entry_point_kind": "external_process",
  "entry_point": "bin/${plugin_binary_name}",
  "package_paths": ["bin/${plugin_binary_name}"]
}
EOF

python3 - <<'PY' "${catalog_path}" "${runtime_manifest_path}" "${checksums_path}" "${runtime_id}" "${package_sha}" "${package_name}"
import json
import sys
from pathlib import Path

catalog_path = Path(sys.argv[1])
manifest_path = Path(sys.argv[2])
checksums_path = Path(sys.argv[3])
runtime_id = sys.argv[4]
package_sha = sys.argv[5]
package_name = sys.argv[6]

with manifest_path.open("r", encoding="utf-8") as handle:
    manifest = json.load(handle)

catalog_entry = {
    "id": manifest["id"],
    "name": manifest["name"],
    "description": manifest["description"],
    "version": manifest["version"],
    "provider_ids": manifest.get("provider_ids", []),
    "platforms": manifest.get("platforms", []),
    "architectures": manifest.get("architectures", []),
    "remote_manifest_url": manifest["remote_manifest_url"],
    "entry_point_kind": manifest.get("entry_point_kind", ""),
    "entry_point": Path(manifest.get("entry_point", "")).name,
}

def runtime_key(entry: dict) -> tuple[str, tuple[str, ...], tuple[str, ...]]:
    platforms = tuple(sorted((entry.get("platforms") or ["any"])))
    architectures = tuple(sorted((entry.get("architectures") or ["any"])))
    return entry.get("id", ""), platforms, architectures

entries = []
if catalog_path.exists():
    with catalog_path.open("r", encoding="utf-8") as handle:
        existing = json.load(handle)
    if isinstance(existing, dict) and isinstance(existing.get("plugins"), list):
        entries = [entry for entry in existing["plugins"] if isinstance(entry, dict)]

merged = {runtime_key(entry): entry for entry in entries}
merged[runtime_key(catalog_entry)] = catalog_entry
catalog_path.write_text(
    json.dumps(
        {"plugins": sorted(
            merged.values(),
            key=lambda entry: (
                str(entry.get("id", "")),
                ",".join(entry.get("platforms", []) or ["any"]),
                ",".join(entry.get("architectures", []) or ["any"]),
            ),
        )},
        indent=2,
    ) + "\n",
    encoding="utf-8",
)

checksums = {}
if checksums_path.exists():
    for raw_line in checksums_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 2:
            checksums[parts[-1]] = parts[0]

checksums[f"onedrive/{runtime_id}/{package_name}"] = package_sha
checksums_path.write_text(
    "".join(f"{checksums[path]}  {path}\n" for path in sorted(checksums)),
    encoding="utf-8",
)
PY

cat > "${readme_path}" <<EOF
Storage Plugin Publish Payload
==============================

This directory contains a generated publish payload for the current OneDrive storage plugin build.

Generated values
- Plugin root base URL: ${base_url}
- Category base URL: ${category_base_url}
- Platform: ${platform}
- Architecture: ${architecture}
- Runtime id: ${runtime_id}
- Plugin version: ${plugin_version}
- Build directory: ${build_dir}

Contents
- catalog.json: merged server-side storage plugin catalog for all runtime variants
- onedrive/${runtime_id}/manifest.json: remote OneDrive plugin manifest for ${platform}/${architecture}
- onedrive/${runtime_id}/${package_name}: installable plugin archive for ${platform}/${architecture}
- SHA256SUMS: merged checksums for all runtime variants under this category

Publish steps
1. Repeat this script once per supported OS/architecture build.
2. Upload the full storage/ directory to your server so the URLs map 1:1.
3. Point the app at:
   ${category_base_url}/catalog.json

Example:
  AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL=${category_base_url}/catalog.json
EOF

echo "[INFO] Generated storage plugin payload in ${output_dir}"
echo "[INFO] Catalog URL: ${category_base_url}/catalog.json"
echo "[INFO] Package SHA-256: ${package_sha}"
