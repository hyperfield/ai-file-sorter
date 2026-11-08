#!/usr/bin/env python3
"""Filter RPM requirements satisfied by the package's private ELF libraries."""

import argparse
import os
from pathlib import Path
import re
import subprocess


SONAME_PATTERN = re.compile(r"\(SONAME\).*?\[(.*?)\]")
ELF_MAGIC = b"\x7fELF"


def bundled_sonames(root):
    """Inspect real ELF files so symlink names cannot hide system dependencies."""
    if not root.is_dir():
        raise ValueError(f"Bundled runtime directory does not exist: {root}")
    sonames = set()
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            if stream.read(len(ELF_MAGIC)) != ELF_MAGIC:
                continue
        output = subprocess.check_output(
            ["readelf", "-W", "-d", str(path)], text=True,
            env={**os.environ, "LC_ALL": "C"},
        )
        sonames.update(SONAME_PATTERN.findall(output))
    return sonames


def dependency_pattern(sonames):
    """Match exact bundled SONAMEs, including versioned RPM dependency forms."""
    if not sonames:
        return None
    names = "|".join(re.escape(name) for name in sorted(set(sonames)))
    return "^(" + names + ")($|[(])"


def spec_filter(sonames):
    """Escape the dependency expression for the RPM spec parser."""
    pattern = dependency_pattern(sonames)
    if pattern is None:
        return ""
    escaped = pattern.replace("\\", "\\\\").replace("%", "%%")
    return "%global __requires_exclude " + escaped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime_root", type=Path)
    args = parser.parse_args()
    try:
        value = spec_filter(bundled_sonames(args.runtime_root))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Cannot inspect bundled RPM dependencies: {error}\n")
    if value:
        print(value)


if __name__ == "__main__":
    main()
