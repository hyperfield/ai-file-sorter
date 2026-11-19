"""Build real pacman archives from isolated fixtures and inspect their contracts."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
RUNTIME_LIBRARIES = (
    "libllama.so.0", "libllama-common.so", "libggml.so.0",
    "libggml-base.so.0", "libggml-cpu.so", "libmtmd.so.0",
)
TOOLS = ("bsdtar", "patchelf", "cc", "desktop-file-validate")


@unittest.skipUnless(all(shutil.which(tool) for tool in TOOLS), "Arch packaging tools")
class ArchPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="aifs-arch-library-")
        cls.addClassCleanup(temporary.cleanup)
        cls.library = Path(temporary.name) / "libfixture.so"
        subprocess.run(
            ["cc", "-shared", "-fPIC", "-x", "c", "-", "-o", str(cls.library)],
            input="int fixture(void) { return 0; }\n", text=True, check=True,
        )

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="aifs-arch-package-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for name in (
            "LICENSE", "app/scripts/package_arch.sh", "app/scripts/gen_run_wrapper.py",
            "app/scripts/run_aifilesorter.sh.in", "app/resources/linux/aifilesorter.desktop",
        ):
            self.copy_input(name)
        for size in (16, 32, 64, 128, 256, 512):
            self.copy_input(f"app/resources/images/icon_{size}x{size}.png")
        header = self.root / "app/include/app_version.hpp"
        header.parent.mkdir(parents=True)
        header.write_text("const Version APP_VERSION({2, 0, 0});\n")
        self.binary = self.root / "app/bin/aifilesorter-bin"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_text("#!/bin/sh\nexit 0\n")
        self.binary.chmod(0o755)
        self.runtime("cpu")

    def copy_input(self, name):
        destination = self.root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, destination)

    def runtime(self, variant):
        directory = self.root / "app/lib/precompiled" / variant / "bin"
        directory.mkdir(parents=True)
        libraries = list(RUNTIME_LIBRARIES)
        if variant != "cpu":
            libraries.append(f"libggml-{variant}.so")
        for name in libraries:
            shutil.copy2(self.library, directory / name)
        return directory

    def package(self, *arguments, **environment):
        return subprocess.run(
            ["bash", str(self.root / "app/scripts/package_arch.sh"), *arguments],
            env={**os.environ, "PKGREL": "1", "PKG_ARCH": "x86_64",
                 "PACKAGER": "AI File Sorter Team <info@filesorter.app>",
                 "SOURCE_DATE_EPOCH": "1700000000", **environment},
            capture_output=True, text=True,
        )

    def archive(self):
        packages = list((self.root / "dist/aifilesorter_arch/final").glob("*.pkg.tar.zst"))
        self.assertEqual(len(packages), 1)
        return packages[0]

    def member(self, name):
        return subprocess.check_output(
            ["bsdtar", "-xOf", str(self.archive()), name], text=True,
        )

    def names(self):
        return subprocess.check_output(
            ["bsdtar", "-tf", str(self.archive())], text=True,
        ).splitlines()

    def test_cpu_package_has_launcher_desktop_icons_and_system_dependencies(self):
        self.runtime("cuda")
        self.runtime("vulkan")
        result = self.package("--cpu-only")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.archive().name, "aifilesorter-2.0.0-1-x86_64.pkg.tar.zst")
        metadata = self.member(".PKGINFO").splitlines()
        for line in (
            "packager = AI File Sorter Team <info@filesorter.app>",
            "depend = ca-certificates", "depend = libwebp", "depend = qt6-base",
            "depend = hicolor-icon-theme",
        ):
            self.assertIn(line, metadata)
        self.assertNotIn("depend = cuda", metadata)
        names = self.names()
        self.assertIn(".MTREE", names)
        self.assertIn("usr/bin/aifilesorter", names)
        self.assertIn("usr/share/licenses/aifilesorter/LICENSE", names)
        self.assertFalse(any("/precompiled/cuda/" in name for name in names))
        self.assertFalse(any("/precompiled/vulkan/" in name for name in names))
        self.assertFalse(any("cacert.pem" in name for name in names))
        for size in (16, 32, 64, 128, 256, 512):
            self.assertIn(f"usr/share/icons/hicolor/{size}x{size}/apps/aifilesorter.png", names)
        desktop = self.member("usr/share/applications/aifilesorter.desktop")
        self.assertIn("Exec=aifilesorter\n", desktop)
        self.assertIn("Icon=aifilesorter\n", desktop)
        self.assertIn('APP_DIR="/opt/aifilesorter"', self.member("usr/bin/run_aifilesorter.sh"))
        if shutil.which("pacman"):
            parsed = subprocess.check_output(["pacman", "-Qp", str(self.archive())], text=True)
            self.assertEqual(parsed.strip(), "aifilesorter 2.0.0-1")

    def test_combined_package_keeps_gpu_host_dependencies_optional(self):
        self.runtime("cuda")
        self.runtime("vulkan")
        result = self.package("--include-all")
        self.assertEqual(result.returncode, 0, result.stderr)
        metadata = self.member(".PKGINFO")
        self.assertIn("optdepend = cuda:", metadata)
        self.assertIn("optdepend = nvidia-utils:", metadata)
        self.assertIn("optdepend = vulkan-icd-loader:", metadata)
        names = self.names()
        for variant in ("cpu", "cuda", "vulkan"):
            self.assertIn(f"opt/aifilesorter/lib/precompiled/{variant}/bin/libggml-cpu.so", names)

    def test_missing_cpu_dependency_is_rejected_before_packaging(self):
        (self.root / "app/lib/precompiled/cpu/bin/libmtmd.so.0").unlink()
        result = self.package("--cpu-only")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Incomplete cpu runtime", result.stderr)
        self.assertFalse((self.root / "dist").exists())

    def test_missing_gpu_backend_is_rejected(self):
        directory = self.runtime("vulkan")
        (directory / "libggml-vulkan.so").unlink()
        result = self.package("--include-vulkan")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Incomplete vulkan runtime", result.stderr)

    def test_metadata_overrides_and_prerelease_version(self):
        result = self.package("--cpu-only", "2.0.0-rc1", PKGREL="2.1", PACKAGER="Test Builder")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("pkgver = 2.0.0_rc1-2.1\n", self.member(".PKGINFO"))
        self.assertIn("packager = Test Builder\n", self.member(".PKGINFO"))

    def test_invalid_metadata_and_conflicting_flags_are_rejected(self):
        for arguments, environment in (
            (("--cpu-only", "--include-vulkan"), {}),
            (("../bad-version",), {}),
            ((), {"PKGREL": "0"}),
            ((), {"PACKAGER": "Test\ndepend = unwanted"}),
        ):
            with self.subTest(arguments=arguments, environment=environment):
                result = self.package(*arguments, **environment)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.root / "dist").exists())


if __name__ == "__main__":
    unittest.main()
