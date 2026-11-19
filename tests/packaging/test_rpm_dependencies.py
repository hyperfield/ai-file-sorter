"""Check private RPM SONAME filtering without hiding ordinary host libraries."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "app" / "scripts"))
from rpm_dependencies import bundled_sonames, dependency_pattern, spec_filter  # noqa: E402


class RpmDependencyTests(unittest.TestCase):
    def test_bundled_library_and_symbol_versions_are_filtered(self):
        pattern = dependency_pattern({"libggml-base.so.0", "libpdfium.so.0"})
        for dependency in (
            "libggml-base.so.0", "libggml-base.so.0()(64bit)",
            "libpdfium.so.0(PDFIUM_1)(64bit)",
        ):
            with self.subTest(dependency=dependency):
                self.assertRegex(dependency, pattern)

    def test_other_sonames_and_host_libraries_remain_required(self):
        pattern = dependency_pattern({"libggml.so.0"})
        for dependency in (
            "libggml.so.1()(64bit)", "libggml.so.00()(64bit)",
            "libggmlXsoX0()(64bit)", "libc.so.6(GLIBC_2.34)(64bit)",
            "libQt6Core.so.6()(64bit)", "libopenblas.so.0()(64bit)",
            "libstdc++.so.6()(64bit)",
        ):
            with self.subTest(dependency=dependency):
                self.assertNotRegex(dependency, pattern)

    def test_empty_payload_cannot_create_a_broad_filter(self):
        self.assertIsNone(dependency_pattern(set()))
        self.assertEqual(spec_filter(set()), "")

    def test_names_are_literal_and_spec_escaping_round_trips(self):
        names = {"libprivate++.so.0", "libprivate.so.0"}
        macro = spec_filter(names)
        expanded = macro.removeprefix("%global __requires_exclude ").replace("\\\\", "\\")
        self.assertRegex("libprivate++.so.0()(64bit)", expanded)
        self.assertNotRegex("libprivate.so.00()(64bit)", expanded)
        self.assertEqual(spec_filter(list(names) * 2), spec_filter(names))

    @unittest.skipUnless(shutil.which("gcc") and shutil.which("readelf"), "ELF toolchain")
    def test_sonames_come_from_elf_metadata_not_filenames_or_symlinks(self):
        with tempfile.TemporaryDirectory(prefix="aifs-rpm-sonames-") as temporary:
            root = Path(temporary)
            source = root / "runtime.c"
            source.write_text("int runtime(void) { return 0; }\n")
            library = root / "misleading-filename.so"
            subprocess.run([
                "gcc", "-shared", "-fPIC", "-Wl,-soname,libprivate.so.7",
                "-o", str(library), str(source),
            ], check=True)
            (root / "libc.so.6").symlink_to(library.name)
            (root / "libtext.so.0").write_text("not an ELF library")
            self.assertEqual(bundled_sonames(root), {"libprivate.so.7"})

    def test_missing_runtime_directory_is_an_error(self):
        with tempfile.TemporaryDirectory(prefix="aifs-rpm-missing-") as temporary:
            with self.assertRaisesRegex(ValueError, "does not exist"):
                bundled_sonames(Path(temporary) / "missing")


if __name__ == "__main__":
    unittest.main()
