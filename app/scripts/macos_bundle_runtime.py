#!/usr/bin/env python3
"""Stage and audit the actual Mach-O dependency closure of a macOS app bundle."""

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


SYSTEM_ROOTS = ("/usr/lib/", "/System/Library/", "/Library/Apple/")
MACHO_MAGICS = {bytes.fromhex(value) for value in (
    "feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca",
    "cafebabf", "bfbafeca")}
LOAD_COMMANDS = {"LC_LOAD_DYLIB", "LC_LOAD_WEAK_DYLIB", "LC_REEXPORT_DYLIB",
                 "LC_LOAD_UPWARD_DYLIB", "LC_LAZY_LOAD_DYLIB"}


def run(*command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def is_system(path):
    return str(path).startswith(SYSTEM_ROOTS)


def inside(path, directory):
    return path.resolve().is_relative_to(directory.resolve())


@dataclass(frozen=True)
class Image:
    identity: str | None
    dependencies: tuple[str, ...]
    rpaths: tuple[str, ...]


def inspect(path):
    identity = None
    dependencies, rpaths = [], []
    for block in re.split(r"Load command \d+\n", run("otool", "-l", str(path)))[1:]:
        command = re.search(r"^\s*cmd (\S+)$", block, re.MULTILINE)
        name = re.search(r"^\s*(?:name|path) (.+) \(offset \d+\)$", block, re.MULTILINE)
        if not command or not name:
            continue
        kind, value = command[1], name[1]
        if kind == "LC_ID_DYLIB":
            identity = value
        elif kind in LOAD_COMMANDS:
            dependencies.append(value)
        elif kind == "LC_RPATH":
            rpaths.append(value)
    return Image(identity, tuple(dict.fromkeys(dependencies)), tuple(dict.fromkeys(rpaths)))


def macho_files(bundle):
    images = set()
    for path in bundle.rglob("*"):
        if path.is_symlink() and not inside(path, bundle):
            raise RuntimeError(f"Bundle symlink escapes its bundle: {path}")
        if path.is_file():
            with path.open("rb") as stream:
                if stream.read(4) in MACHO_MAGICS:
                    images.add(path.resolve())
    return sorted(images)


def expand_path(value, loader, executable):
    for token, directory in (("@loader_path", loader.parent),
                             ("@executable_path", executable.parent)):
        if value == token or value.startswith(token + "/"):
            return directory / value.removeprefix(token).lstrip("/")
    return Path(value)


class DependencyClosure:
    def __init__(self, bundle, executable, runtime, source_executable=None):
        self.bundle = bundle.resolve()
        self.executable = executable.resolve()
        self.frameworks = self.bundle / "Contents/Frameworks"
        self.runtime = self.bundle / "Contents/lib" / runtime
        self.source_executable = (source_executable or executable).resolve()
        if not inside(self.executable, self.bundle) or not inside(self.runtime, self.bundle):
            raise RuntimeError("Executable and runtime directory must be inside the bundle")
        self.origins = {self.executable: self.source_executable}
        self.images = {}
        self.edges = {}

    def destination(self, source):
        framework = next((part for part in source.parents if part.suffix == ".framework"), None)
        if framework:
            return self.frameworks / framework.name / source.relative_to(framework)
        return self.frameworks / source.name

    def stage(self, source):
        source = source.resolve()
        if inside(source, self.bundle):
            return source
        staged = self.runtime / source.name
        if staged.is_file() and staged.read_bytes() == source.read_bytes():
            self.origins[staged.resolve()] = source
            return staged.resolve()
        target = self.destination(source)
        if target.exists():
            if target.read_bytes() != source.read_bytes():
                raise RuntimeError(f"Conflicting dependency destinations: {source} -> {target}")
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            print(f"Copied runtime: {source} -> {target}")
        self.origins[target.resolve()] = source
        return target.resolve()

    def resolve(self, dependency, loader, image):
        if dependency.startswith("/"):
            # Keep the exact dependency/ABI; never substitute a library by a
            # similar basename or rewrite a system library to a bundled one.
            source = Path(dependency)
            if source.is_file():
                return self.stage(source)
        else:
            source_loader = self.origins.get(loader, loader)
            # Resolve the original link first, before considering staged files
            # with the same basename (which might be a different build/ABI).
            searches = [expand_path(value, source_loader, self.source_executable)
                        for value in image.rpaths]
            searches += [expand_path(value, self.source_executable, self.source_executable)
                         for value in inspect(self.executable).rpaths]
            searches += [self.frameworks, self.runtime]
            if dependency.startswith("@rpath/"):
                candidates = [directory / dependency.removeprefix("@rpath/") for directory in searches]
            else:
                candidates = [expand_path(dependency, source_loader, self.source_executable)]
            for candidate in candidates:
                if candidate.is_file():
                    return self.stage(candidate)
        raise RuntimeError(f"Missing runtime dependency: {loader} -> {dependency}")

    def collect(self):
        pending = macho_files(self.bundle)
        while pending:
            loader = pending.pop()
            if loader in self.images:
                continue
            image = inspect(loader)
            self.images[loader] = image
            for dependency in image.dependencies:
                if is_system(dependency):
                    continue
                target = self.resolve(dependency, loader, image)
                self.edges[loader, dependency] = target
                pending.append(target)

    def reference(self, path):
        for directory in (self.frameworks, self.runtime):
            if inside(path, directory):
                return "@rpath/" + str(path.relative_to(directory))
        raise RuntimeError(f"Runtime is outside library directories: {path}")

    def rewrite(self, architecture):
        expected = {"arm64", "x86_64"} if architecture == "universal" else {architecture}
        for path, image in self.images.items():
            actual = set(run("lipo", "-archs", str(path)).split())
            if not expected <= actual:
                raise RuntimeError(f"Architecture mismatch: {path}: expected {expected}, got {actual}")
            path.chmod(path.stat().st_mode | 0o200)
            if image.identity:
                run("install_name_tool", "-id", self.reference(path), str(path))
            for dependency in image.dependencies:
                if not is_system(dependency):
                    run("install_name_tool", "-change", dependency,
                        self.reference(self.edges[path, dependency]), str(path))
            # Discard all inherited run paths, including relative paths which
            # could escape after relocation. Each image gets its own local roots.
            for value in image.rpaths:
                run("install_name_tool", "-delete_rpath", value, str(path))
            for directory in (self.frameworks, self.runtime):
                relative = os.path.relpath(directory, path.parent)
                run("install_name_tool", "-add_rpath", "@loader_path/" + relative, str(path))

    def audit(self):
        for path in self.images:
            image = inspect(path)
            for value in image.rpaths:
                if not inside(expand_path(value, path, self.executable), self.bundle):
                    raise RuntimeError(f"External runtime search path: {path} -> {value}")
            for dependency in image.dependencies:
                if is_system(dependency):
                    continue
                if not dependency.startswith("@rpath/"):
                    raise RuntimeError(f"External runtime dependency: {path} -> {dependency}")
                suffix = dependency.removeprefix("@rpath/")
                if not any((directory / suffix).is_file() for directory in (self.frameworks, self.runtime)):
                    raise RuntimeError(f"Unresolved bundled dependency: {path} -> {dependency}")


def verify_loaded(bundle, executable):
    environment = {key: value for key, value in os.environ.items() if not key.startswith("DYLD_")}
    with tempfile.TemporaryDirectory(prefix="aifs-package-check-") as temporary:
        root = Path(temporary)
        environment.update(AI_FILE_SORTER_CONFIG_DIR=str(root / "config"),
                           XDG_CONFIG_HOME=str(root / "xdg-config"),
                           XDG_CACHE_HOME=str(root / "xdg-cache"))
        command = [str(executable.resolve()), "--cuda=off", "--vulkan=off", "--self-test=tls"]
        result = subprocess.run(command, cwd="/", env=environment, capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        result.check_returncode()
        environment["DYLD_PRINT_LIBRARIES"] = "1"
        observed = subprocess.run(command, cwd="/", env=environment, capture_output=True, text=True, timeout=60)
        observed.check_returncode()
        loaded = re.findall(r"^dyld\[\d+\]:\s+(?:<[^>]+>\s+)?(/.+)$", observed.stderr, re.MULTILINE)
        if not loaded:
            raise RuntimeError("dyld did not report loaded images; packaged runtime could not be verified")
        for path in loaded:
            if not is_system(path) and not inside(Path(path), bundle):
                raise RuntimeError(f"Packaged application loaded an external runtime: {path}")
        if list(bundle.rglob("cacert.pem")):
            raise RuntimeError("macOS bundle must not contain cacert.pem")
        print(f"Verified packaged TLS policy and {len(loaded)} loaded images from /")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "verify-loaded"))
    parser.add_argument("--bundle", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--runtime-subdir", default="precompiled")
    parser.add_argument("--source-executable", type=Path, help="Original executable before staging")
    parser.add_argument("--arch", choices=("arm64", "x86_64", "universal"))
    parser.add_argument("--files", type=Path, help="Write a NUL-delimited signing inventory")
    args = parser.parse_args()
    if args.action == "verify-loaded":
        verify_loaded(args.bundle, args.executable)
        return
    if not args.arch or not args.files:
        parser.error("prepare requires --arch and --files")
    closure = DependencyClosure(args.bundle, args.executable, args.runtime_subdir, args.source_executable)
    closure.collect()
    closure.rewrite(args.arch)
    closure.audit()
    args.files.write_bytes(b"".join(os.fsencode(path) + b"\0" for path in sorted(closure.images)))
    print(f"Prepared {len(closure.images)} bundle images ({args.arch})")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f"macOS bundle runtime check failed: {error}")
