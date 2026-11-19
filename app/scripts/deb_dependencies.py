#!/usr/bin/env python3
"""Derive Debian dependencies and reject payloads newer than the build system."""

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


PACKAGE_NAME = "aifilesorter"
VERSION_PATTERN = re.compile(r"Name: (GLIBCXX|GLIBC|CXXABI|Qt)_(\d+(?:\.\d+)+)\b")
OPTIONAL_CUDA_LIBRARIES = ("libcuda", "libcudart", "libcublas", "libcublasLt")


@dataclass(frozen=True)
class ElfArtifact:
    """A staged ELF file and its directly imported libraries and symbol versions."""

    path: Path
    versions: dict
    needed: tuple


def command_output(arguments, **kwargs):
    return subprocess.check_output(
        arguments, text=True, env={**os.environ, "LC_ALL": "C"}, **kwargs,
    ).strip()


def parse_versions(text):
    """Return the highest numeric symbol version for each ABI family."""
    versions = {}
    for family, value in VERSION_PATTERN.findall(text):
        version = tuple(map(int, value.split(".")))
        versions[family] = max(versions.get(family, ()), version)
    return versions


def parse_required_versions(text):
    """Ignore exported version definitions when inspecting a payload file."""
    _definitions, separator, needs = text.partition("Version needs section")
    return parse_versions(needs) if separator else {}


def inspect_artifacts(package_root):
    artifacts = []
    for path in sorted(package_root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as handle:
            if handle.read(4) != b"\x7fELF":
                continue
        versions = command_output(["readelf", "-W", "--version-info", str(path)])
        dynamic = command_output(["readelf", "-W", "-d", str(path)])
        needed = tuple(re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic))
        artifacts.append(ElfArtifact(path, parse_required_versions(versions), needed))
    if not artifacts:
        raise ValueError(f"No ELF artifacts found under {package_root}.")
    return artifacts


def available_versions():
    glibc = command_output(["getconf", "GNU_LIBC_VERSION"]).split()[-1]
    qt = command_output(["pkg-config", "--modversion", "Qt6Core"])
    stdcpp = command_output(["c++", "-print-file-name=libstdc++.so.6"])
    versions = parse_versions(command_output(["readelf", "-W", "--version-info", stdcpp]))
    versions["GLIBC"] = tuple(map(int, glibc.split(".")))
    versions["Qt"] = tuple(map(int, qt.split(".")))
    return versions


def validate_versions(artifacts, available):
    """Reject newer staged app, CPU, GPU, or document-library ABIs."""
    for artifact in artifacts:
        for family, required in artifact.versions.items():
            provided = available.get(family, ())
            if required > provided:
                required_text = ".".join(map(str, required))
                provided_text = ".".join(map(str, provided)) or "unavailable"
                raise ValueError(
                    f"{artifact.path} requires {family}_{required_text}, but the build "
                    f"system provides {family}_{provided_text}. Rebuild every runtime "
                    "on the supported distribution before packaging."
                )


def optional_cuda_shlibs(artifacts):
    """Keep CUDA toolkit/driver dependencies optional for CPU-only machines."""
    entries = set()
    for artifact in artifacts:
        for soname in artifact.needed:
            name, separator, version = soname.partition(".so.")
            if separator and name in OPTIONAL_CUDA_LIBRARIES:
                entries.add(f"{name} {version} {PACKAGE_NAME}")
    return "".join(entry + "\n" for entry in sorted(entries))


def generate_dependencies(package_root, artifacts, library_dirs):
    with tempfile.TemporaryDirectory(prefix="aifs-deb-dependencies-") as temporary:
        source = Path(temporary)
        debian = source / "debian"
        debian.mkdir()
        (debian / "control").write_text(
            f"Source: {PACKAGE_NAME}\n\nPackage: {PACKAGE_NAME}\n"
            "Architecture: any\nDescription: AI File Sorter\n"
        )
        # Only CUDA's external toolkit/driver SONAMEs map to the excluded
        # self-dependency. Their ELF files still contribute libc/C++ requirements.
        (debian / "shlibs.local").write_text(optional_cuda_shlibs(artifacts))
        directories = {artifact.path.parent for artifact in artifacts}
        directories.update(path.resolve() for path in library_dirs if path.is_dir())
        arguments = ["dpkg-shlibdeps", "-O", f"-x{PACKAGE_NAME}", f"-S{package_root}"]
        arguments.extend(f"-l{path}" for path in sorted(directories))
        arguments.extend(f"-e{artifact.path}" for artifact in artifacts)
        result = command_output(arguments, cwd=source)
    for line in result.splitlines():
        if line.startswith("shlibs:Depends="):
            return line.removeprefix("shlibs:Depends=")
    raise ValueError("dpkg-shlibdeps did not produce system-library dependencies.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package_root", type=Path)
    parser.add_argument("--library-dir", type=Path, action="append", default=[])
    args = parser.parse_args()
    try:
        root = args.package_root.resolve()
        artifacts = inspect_artifacts(root)
        validate_versions(artifacts, available_versions())
        print(generate_dependencies(root, artifacts, args.library_dir))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Cannot generate Debian dependencies: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
