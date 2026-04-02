#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import platform as py_platform
import posixpath
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_PAYLOAD_DIR = REPO_ROOT / "plugins"
DEFAULT_BUILD_DIR = REPO_ROOT / "build-tests"
PLUGIN_TARGETS_PATH = SCRIPT_DIR / "plugin_build_targets.tsv"


def fail(message: str) -> None:
    print(f"[ERROR] {message}", file=sys.stderr)
    raise SystemExit(1)


def info(message: str) -> None:
    print(f"[INFO] {message}")


def normalize_url(value: str) -> str:
    while value.endswith("/"):
        value = value[:-1]
    return value


def split_remote_dir(remote_dir: str) -> tuple[str | None, str]:
    if ":" not in remote_dir:
        return None, remote_dir
    host, path = remote_dir.split(":", 1)
    return host, path


def remote_parent_dir(remote_dir: str) -> str | None:
    host, path = split_remote_dir(remote_dir)
    normalized = path.rstrip("/")
    if not normalized:
        return None
    parent = posixpath.dirname(normalized)
    if not parent or parent == normalized:
        return None
    if host:
        return f"{host}:{parent}"
    return parent


def format_rsync_error(remote_dir: str, output: str, returncode: int) -> str:
    message_lines = [
        f"Failed to upload plugin payload with rsync (exit code {returncode}).",
    ]

    if "mkdir" in output and "No such file or directory" in output:
        parent = remote_parent_dir(remote_dir)
        message_lines.append("The remote destination directory does not exist yet.")
        message_lines.append(f"Configured destination: {remote_dir}")
        if parent:
            host, path = split_remote_dir(parent)
            if host:
                message_lines.append(f"Create it first, for example: ssh {host} 'mkdir -p {path}'")
            else:
                message_lines.append(f"Create it first, for example: mkdir -p {parent}")
    elif "Permission denied" in output:
        message_lines.append("The remote server rejected the upload due to missing permissions.")
        message_lines.append(f"Configured destination: {remote_dir}")
    else:
        message_lines.append(f"Configured destination: {remote_dir}")

    trimmed_output = output.strip()
    if trimmed_output:
        message_lines.append("")
        message_lines.append("rsync output:")
        message_lines.append(trimmed_output)

    return "\n".join(message_lines)


def current_platform() -> str:
    system = py_platform.system().lower()
    if system == "darwin":
        return "macos"
    if system == "windows":
        return "windows"
    if system == "linux":
        return "linux"
    return system


def current_architecture() -> str:
    machine = py_platform.machine().lower()
    aliases = {
        "amd64": "x86_64",
        "x64": "x86_64",
        "aarch64": "arm64",
        "i386": "x86",
        "i686": "x86",
    }
    return aliases.get(machine, machine)


def normalize_list(values: Any) -> list[str]:
    if values is None:
        return []
    if isinstance(values, str):
        values = [values]
    result: list[str] = []
    seen: set[str] = set()
    for value in values:
        if not isinstance(value, str):
            continue
        normalized = value.strip().lower()
        if not normalized:
            continue
        if normalized not in seen:
            seen.add(normalized)
            result.append(normalized)
    return result


def runtime_key(manifest: dict[str, Any]) -> tuple[str, tuple[str, ...], tuple[str, ...]]:
    plugin_id = str(manifest.get("id", "")).strip()
    platforms = tuple(sorted(normalize_list(manifest.get("platforms")) or ["any"]))
    architectures = tuple(sorted(normalize_list(manifest.get("architectures")) or ["any"]))
    return plugin_id, platforms, architectures


def compare_versions(left: str, right: str) -> int:
    def parse(version: str) -> list[int] | None:
        parts = version.split(".")
        digits: list[int] = []
        for part in parts:
            if not part.isdigit():
                return None
            digits.append(int(part))
        return digits

    lhs = parse(left)
    rhs = parse(right)
    if lhs is None or rhs is None:
        if left == right:
            return 0
        return 1 if left > right else -1

    max_len = max(len(lhs), len(rhs))
    lhs.extend([0] * (max_len - len(lhs)))
    rhs.extend([0] * (max_len - len(rhs)))
    if lhs == rhs:
        return 0
    return 1 if lhs > rhs else -1


def fetch_url(url: str) -> bytes | None:
    request = urllib.request.Request(url, headers={"User-Agent": "AIFileSorterPluginUploader/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return response.read()
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            return None
        raise
    except urllib.error.URLError:
        raise


def load_json_file(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def fetch_json_url(url: str) -> dict[str, Any] | None:
    payload = fetch_url(url)
    if payload is None:
        return None
    return json.loads(payload.decode("utf-8"))


def fetch_text_url(url: str) -> str | None:
    payload = fetch_url(url)
    if payload is None:
        return None
    return payload.decode("utf-8")


def compute_sha256(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_catalog(payload: dict[str, Any]) -> list[dict[str, Any]]:
    if isinstance(payload, list):
        return [entry for entry in payload if isinstance(entry, dict)]
    plugins = payload.get("plugins")
    if isinstance(plugins, list):
        return [entry for entry in plugins if isinstance(entry, dict)]
    if payload:
        return [payload]
    return []


def serialize_catalog(entries: list[dict[str, Any]]) -> str:
    return json.dumps({"plugins": entries}, indent=2) + "\n"


def parse_checksums(text: str | None) -> dict[str, str]:
    checksums: dict[str, str] = {}
    if not text:
        return checksums
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        checksums[parts[-1]] = parts[0]
    return checksums


def serialize_checksums(entries: dict[str, str]) -> str:
    lines = [f"{entries[path]}  {path}" for path in sorted(entries)]
    return "\n".join(lines) + ("\n" if lines else "")


def ensure_url_under_base(url: str, base_url: str, label: str) -> None:
    normalized_base = normalize_url(base_url)
    if not url.startswith(normalized_base + "/"):
        fail(f"{label} does not live under the configured base URL: {url}")


@dataclass
class BuildTarget:
    plugin_id: str
    display_name: str
    cmake_target: str
    output_stem: str
    platforms: set[str]


@dataclass
class LocalPluginPayload:
    category: str
    plugin_id: str
    runtime_id: str
    display_name: str
    manifest_path: Path
    manifest: dict[str, Any]
    catalog_entry: dict[str, Any]
    package_path: Path
    package_relative_path: str
    package_sha256: str

    @property
    def selection_key(self) -> str:
        return f"{self.category}/{self.plugin_id}/{self.runtime_id}"


def load_build_targets() -> dict[str, BuildTarget]:
    targets: dict[str, BuildTarget] = {}
    if not PLUGIN_TARGETS_PATH.exists():
        return targets
    with PLUGIN_TARGETS_PATH.open("r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 5:
                continue
            targets[parts[0]] = BuildTarget(
                plugin_id=parts[0],
                display_name=parts[1],
                cmake_target=parts[2],
                output_stem=parts[3],
                platforms={value.strip().lower() for value in parts[4].split(",") if value.strip()},
            )
    return targets


def find_local_plugin_payloads_for_category(category_dir: Path) -> dict[str, LocalPluginPayload]:
    category = category_dir.name
    catalog_path = category_dir / "catalog.json"
    if not catalog_path.exists():
        fail(f"Missing payload catalog: {catalog_path}")

    catalog_entries = parse_catalog(load_json_file(catalog_path))
    catalog_by_key = {runtime_key(entry): entry for entry in catalog_entries}

    payloads: dict[str, LocalPluginPayload] = {}
    for manifest_path in sorted(category_dir.glob("*/*/manifest.json")):
        manifest = load_json_file(manifest_path)
        plugin_id = str(manifest.get("id", "")).strip()
        if not plugin_id:
            fail(f"Manifest is missing plugin id: {manifest_path}")
        runtime_id = manifest_path.parent.name

        key = runtime_key(manifest)
        catalog_entry = catalog_by_key.get(key)
        if catalog_entry is None:
            fail(f"Payload catalog.json is missing entry for {plugin_id} ({runtime_id})")

        package_url = str(manifest.get("package_download_url", "")).strip()
        if not package_url:
            fail(f"Manifest is missing package_download_url: {manifest_path}")

        package_name = Path(package_url).name
        package_path = manifest_path.parent / package_name
        if not package_path.exists():
            fail(f"Manifest package file is missing: {package_path}")

        package_relative = str(Path(manifest_path.parent.parent.name) / runtime_id / package_name).replace("\\", "/")
        payload = LocalPluginPayload(
            category=category,
            plugin_id=plugin_id,
            runtime_id=runtime_id,
            display_name=str(manifest.get("name", plugin_id)),
            manifest_path=manifest_path,
            manifest=manifest,
            catalog_entry=catalog_entry,
            package_path=package_path,
            package_relative_path=package_relative,
            package_sha256=str(manifest.get("package_sha256", "")).strip().lower(),
        )
        payloads[payload.selection_key] = payload
    return payloads


def find_local_plugin_payloads(payload_root: Path) -> dict[str, LocalPluginPayload]:
    payloads: dict[str, LocalPluginPayload] = {}
    category_dirs = sorted(
        entry for entry in payload_root.iterdir()
        if entry.is_dir() and (entry / "catalog.json").exists()
    )
    if not category_dirs:
        fail(f"No plugin categories with catalog.json were found under: {payload_root}")

    for category_dir in category_dirs:
        for selection_key, payload in find_local_plugin_payloads_for_category(category_dir).items():
            if selection_key in payloads:
                fail(f"Duplicate prepared payload key detected: {selection_key}")
            payloads[selection_key] = payload
    return payloads


def verify_archive(payload: LocalPluginPayload) -> None:
    computed_sha = compute_sha256(payload.package_path)
    if payload.package_sha256 != computed_sha:
        fail(
            f"SHA-256 mismatch for {payload.plugin_id}: manifest has {payload.package_sha256}, "
            f"computed {computed_sha}"
        )

    entry_point = str(payload.manifest.get("entry_point", "")).strip()
    if not entry_point:
        fail(f"Manifest is missing entry_point: {payload.manifest_path}")

    with zipfile.ZipFile(payload.package_path, "r") as archive:
        names = set(archive.namelist())
        if "manifest.json" not in names:
            fail(f"Archive is missing manifest.json: {payload.package_path}")
        if entry_point not in names:
            fail(f"Archive is missing plugin entry point {entry_point}: {payload.package_path}")
        for package_path in payload.manifest.get("package_paths", []):
            if package_path not in names:
                fail(f"Archive is missing package path {package_path}: {payload.package_path}")


def verify_publish_urls(payload: LocalPluginPayload, base_url: str) -> None:
    category_base_url = f"{normalize_url(base_url)}/{payload.category}"
    remote_manifest_url = str(payload.manifest.get("remote_manifest_url", "")).strip()
    package_download_url = str(payload.manifest.get("package_download_url", "")).strip()
    catalog_manifest_url = str(payload.catalog_entry.get("remote_manifest_url", "")).strip()

    if remote_manifest_url:
        ensure_url_under_base(
            remote_manifest_url,
            category_base_url,
            f"{payload.selection_key} manifest remote_manifest_url",
        )
    if package_download_url:
        ensure_url_under_base(
            package_download_url,
            category_base_url,
            f"{payload.selection_key} manifest package_download_url",
        )
    if catalog_manifest_url:
        ensure_url_under_base(
            catalog_manifest_url,
            category_base_url,
            f"{payload.selection_key} catalog remote_manifest_url",
        )


def verify_build_artifact(payload: LocalPluginPayload, build_targets: dict[str, BuildTarget], build_dir: Path) -> None:
    target = build_targets.get(payload.plugin_id)
    if target is None:
        return
    payload_platforms = normalize_list(payload.manifest.get("platforms")) or ["any"]
    payload_architectures = normalize_list(payload.manifest.get("architectures")) or ["any"]
    if "any" not in payload_platforms and current_platform() not in payload_platforms:
        return
    if "any" not in payload_architectures and current_architecture() not in payload_architectures:
        return
    if current_platform() not in target.platforms and "all" not in target.platforms and "any" not in target.platforms:
        return

    output_candidates = [
        build_dir / target.output_stem,
        build_dir / f"{target.output_stem}.exe",
        build_dir / target.cmake_target,
        build_dir / f"{target.cmake_target}.exe",
    ]
    if any(candidate.exists() for candidate in output_candidates):
        return

    fail(
        f"Expected a compiled build artifact for {payload.plugin_id} under {build_dir}, "
        f"but none of these were found: {', '.join(str(path) for path in output_candidates)}"
    )


def choose_plugins_interactively(payloads: dict[str, LocalPluginPayload]) -> list[str]:
    print("Available prepared plugins:")
    for selection_key, payload in payloads.items():
        version = payload.manifest.get("version", "")
        plats = ",".join(normalize_list(payload.manifest.get("platforms")) or ["any"])
        archs = ",".join(normalize_list(payload.manifest.get("architectures")) or ["any"])
        print(f"  {selection_key:<48} {payload.display_name} [{version}] ({plats}/{archs})")
    answer = input(
        "Enter comma-separated plugin IDs, category/plugin IDs, or category/plugin/runtime IDs to upload [all]: "
    ).strip()
    if not answer:
        return list(payloads.keys())
    return [value.strip() for value in answer.split(",") if value.strip()]


def resolve_selected_plugins(payloads: dict[str, LocalPluginPayload], args: argparse.Namespace) -> list[LocalPluginPayload]:
    if args.interactive:
        requested_ids = choose_plugins_interactively(payloads)
    elif args.plugins:
        requested_ids = [value.strip() for chunk in args.plugins for value in chunk.split(",") if value.strip()]
    else:
        requested_ids = list(payloads.keys())

    payloads_by_plugin_id: dict[str, list[LocalPluginPayload]] = {}
    payloads_by_category_plugin: dict[str, list[LocalPluginPayload]] = {}
    for payload in payloads.values():
        payloads_by_plugin_id.setdefault(payload.plugin_id, []).append(payload)
        payloads_by_category_plugin.setdefault(f"{payload.category}/{payload.plugin_id}", []).append(payload)

    selected: list[LocalPluginPayload] = []
    seen_keys: set[str] = set()
    for plugin_id in requested_ids:
        payload = payloads.get(plugin_id)
        if payload is None and plugin_id.count("/") == 1:
            matches = payloads_by_category_plugin.get(plugin_id, [])
            if len(matches) == 1:
                payload = matches[0]
            elif len(matches) > 1:
                for match in sorted(matches, key=lambda item: item.selection_key):
                    if match.selection_key in seen_keys:
                        continue
                    seen_keys.add(match.selection_key)
                    selected.append(match)
                continue
        if payload is None and "/" not in plugin_id:
            matches = payloads_by_plugin_id.get(plugin_id, [])
            if len(matches) == 1:
                payload = matches[0]
            elif len(matches) > 1:
                for match in sorted(matches, key=lambda item: item.selection_key):
                    if match.selection_key in seen_keys:
                        continue
                    seen_keys.add(match.selection_key)
                    selected.append(match)
                continue
        if payload is None:
            fail(f"Prepared payload does not include plugin id: {plugin_id}")
        if payload.selection_key in seen_keys:
            continue
        seen_keys.add(payload.selection_key)
        selected.append(payload)
    return selected


def fetch_remote_catalog(base_url: str, category: str, assume_empty: bool) -> list[dict[str, Any]]:
    if assume_empty:
        return []
    catalog = fetch_json_url(f"{normalize_url(base_url)}/{category}/catalog.json")
    if catalog is None:
        return []
    return parse_catalog(catalog)


def decide_actions(
    selected_payloads: list[LocalPluginPayload],
    remote_catalog: list[dict[str, Any]],
    allow_downgrade: bool,
    force_same_version: bool,
) -> tuple[list[LocalPluginPayload], dict[tuple[str, tuple[str, ...], tuple[str, ...]], dict[str, Any]]]:
    remote_by_key = {runtime_key(entry): entry for entry in remote_catalog}
    to_upload: list[LocalPluginPayload] = []

    for payload in selected_payloads:
        key = runtime_key(payload.manifest)
        remote_entry = remote_by_key.get(key)
        if remote_entry is None:
            info(f"{payload.selection_key}: no remote entry for this runtime; will upload.")
            to_upload.append(payload)
            continue

        remote_manifest = None
        remote_manifest_url = str(remote_entry.get("remote_manifest_url", "")).strip()
        if remote_manifest_url:
            try:
                remote_manifest = fetch_json_url(remote_manifest_url)
            except urllib.error.URLError as exc:
                info(
                    f"{payload.plugin_id}: failed to fetch remote manifest {remote_manifest_url} "
                    f"({exc}); falling back to catalog entry metadata."
                )

        local_version = str(payload.manifest.get("version", "")).strip()
        remote_version = str((remote_manifest or remote_entry).get("version", "")).strip()
        local_sha = payload.package_sha256
        remote_sha = str((remote_manifest or {}).get("package_sha256", "")).strip().lower()

        comparison = compare_versions(local_version, remote_version)
        if comparison < 0 and not allow_downgrade:
            fail(
                f"{payload.selection_key}: local version {local_version} is older than remote {remote_version}. "
                "Use --allow-downgrade to override."
            )

        if comparison == 0:
            if remote_sha and remote_sha == local_sha:
                info(f"{payload.selection_key}: remote version {remote_version} already matches local package; skipping.")
                continue
            if remote_sha and remote_sha != local_sha and not force_same_version:
                fail(
                    f"{payload.selection_key}: remote version {remote_version} has a different package SHA-256. "
                    "Refusing same-version republish without --force-same-version."
                )
            if not remote_sha and not force_same_version:
                info(
                    f"{payload.selection_key}: remote version {remote_version} matches local version "
                    "and no remote package hash is available; skipping."
                )
                continue

        if comparison > 0:
            info(f"{payload.selection_key}: local version {local_version} is newer than remote {remote_version}; will upload.")
        elif comparison < 0:
            info(f"{payload.selection_key}: local version {local_version} is older than remote {remote_version}; will upload.")
        else:
            info(f"{payload.selection_key}: same version but forced package refresh; will upload.")
        to_upload.append(payload)

    return to_upload, remote_by_key


def stage_upload(
    payload_root: Path,
    selected_payloads: list[LocalPluginPayload],
    remote_catalogs: dict[str, list[dict[str, Any]]],
    remote_checksums: dict[str, str | None],
) -> Path:
    stage_root = Path(tempfile.mkdtemp(prefix="aifs-plugin-upload-"))
    selected_by_category: dict[str, list[LocalPluginPayload]] = {}
    for payload in selected_payloads:
        selected_by_category.setdefault(payload.category, []).append(payload)

    for category, category_payloads in selected_by_category.items():
        stage_category = stage_root / category
        stage_category.mkdir(parents=True, exist_ok=True)

        merged_catalog = {runtime_key(entry): entry for entry in remote_catalogs.get(category, [])}
        for payload in category_payloads:
            merged_catalog[runtime_key(payload.catalog_entry)] = payload.catalog_entry

            relative_manifest = payload.manifest_path.relative_to(payload_root / category)
            staged_manifest = stage_category / relative_manifest
            staged_manifest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(payload.manifest_path, staged_manifest)

            relative_package = Path(payload.package_relative_path)
            staged_package = stage_category / relative_package
            staged_package.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(payload.package_path, staged_package)

        merged_entries = sorted(
            merged_catalog.values(),
            key=lambda entry: (
                str(entry.get("id", "")),
                ",".join(normalize_list(entry.get("platforms")) or ["any"]),
                ",".join(normalize_list(entry.get("architectures")) or ["any"]),
            ),
        )
        (stage_category / "catalog.json").write_text(serialize_catalog(merged_entries), encoding="utf-8")

        checksum_entries = parse_checksums(remote_checksums.get(category))
        for payload in category_payloads:
            checksum_entries[payload.package_relative_path] = payload.package_sha256
        (stage_category / "SHA256SUMS").write_text(serialize_checksums(checksum_entries), encoding="utf-8")

    return stage_root


def rsync_upload(stage_root: Path, remote_dir: str, dry_run: bool) -> None:
    command = ["rsync", "-av"]
    if dry_run:
        command.append("--dry-run")
    command.extend([str(stage_root) + "/", remote_dir.rstrip("/") + "/"])

    info("Uploading staged plugin payload with rsync")
    info("Command: " + " ".join(command))
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)
    if completed.returncode != 0:
        combined_output = "\n".join(
            part for part in [completed.stdout.strip(), completed.stderr.strip()] if part
        )
        fail(format_rsync_error(remote_dir, combined_output, completed.returncode))


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Upload prepared plugin payloads to a remote server, "
        "skipping unchanged versions and merging per-category remote catalogs."
    )
    parser.add_argument("--base-url", required=True, help="Public HTTPS base URL for the plugin root.")
    parser.add_argument("--remote-dir", required=True, help="rsync destination for the plugin root.")
    parser.add_argument("--payload-dir", default=str(DEFAULT_PAYLOAD_DIR), help="Local prepared plugin root directory.")
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR), help="Local build directory for compile checks.")
    parser.add_argument("--plugins", action="append", help="Comma-separated plugin IDs to upload.")
    parser.add_argument("--list", action="store_true", help="List prepared plugins and exit.")
    parser.add_argument("--interactive", action="store_true", help="Choose plugins interactively.")
    parser.add_argument("--assume-empty-remote", action="store_true", help="Skip fetching remote catalog and treat the remote as empty.")
    parser.add_argument("--allow-downgrade", action="store_true", help="Allow uploading a lower local version over a higher remote version.")
    parser.add_argument(
        "--force-same-version",
        action="store_true",
        help="Allow uploading when the remote has the same version but a different package hash.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Show what would be uploaded without changing the remote.")
    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()

    base_url = normalize_url(args.base_url)
    payload_dir = Path(args.payload_dir).resolve()
    build_dir = Path(args.build_dir).resolve()

    if not payload_dir.exists():
        fail(f"Payload directory does not exist: {payload_dir}")

    build_targets = load_build_targets()
    payloads = find_local_plugin_payloads(payload_dir)

    if args.list:
        print("Prepared plugins available for upload:")
        for selection_key, payload in payloads.items():
            version = payload.manifest.get("version", "")
            plats = ",".join(normalize_list(payload.manifest.get("platforms")) or ["any"])
            archs = ",".join(normalize_list(payload.manifest.get("architectures")) or ["any"])
            print(f"  {selection_key:<48} {payload.display_name} [{version}] ({plats}/{archs})")
        return 0

    selected_payloads = resolve_selected_plugins(payloads, args)

    for payload in selected_payloads:
        verify_archive(payload)
        verify_publish_urls(payload, base_url)
        verify_build_artifact(payload, build_targets, build_dir)

    remote_catalogs: dict[str, list[dict[str, Any]]] = {}
    remote_checksums: dict[str, str | None] = {}
    to_upload: list[LocalPluginPayload] = []
    for category in sorted({payload.category for payload in selected_payloads}):
        category_payloads = [payload for payload in selected_payloads if payload.category == category]
        remote_catalog = fetch_remote_catalog(base_url, category, args.assume_empty_remote)
        remote_checksum_text = None if args.assume_empty_remote else fetch_text_url(
            f"{normalize_url(base_url)}/{category}/SHA256SUMS"
        )
        remote_catalogs[category] = remote_catalog
        remote_checksums[category] = remote_checksum_text

        category_uploads, _ = decide_actions(
            category_payloads,
            remote_catalog,
            allow_downgrade=args.allow_downgrade,
            force_same_version=args.force_same_version,
        )
        to_upload.extend(category_uploads)

    if not to_upload:
        info("Nothing to upload; all selected plugins already match the remote state.")
        return 0

    stage_root = stage_upload(payload_dir, to_upload, remote_catalogs, remote_checksums)
    try:
        rsync_upload(stage_root, args.remote_dir, args.dry_run)
    finally:
        shutil.rmtree(stage_root, ignore_errors=True)

    info("Plugin upload completed.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except urllib.error.HTTPError as exc:
        fail(f"HTTP {exc.code} while fetching {exc.url}")
    except urllib.error.URLError as exc:
        fail(f"Failed to reach the remote plugin server: {exc.reason}")
    except json.JSONDecodeError as exc:
        fail(f"Received invalid JSON while reading plugin metadata: {exc}")
    except OSError as exc:
        fail(f"File operation failed: {exc}")
