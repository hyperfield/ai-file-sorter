"""Regression coverage for Debian ABI requirements and optional CUDA policy."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "app" / "scripts"))
from deb_dependencies import (  # noqa: E402
    ElfArtifact,
    optional_cuda_shlibs,
    parse_required_versions,
    parse_versions,
    validate_versions,
)


class DebDependencyTests(unittest.TestCase):
    def test_numeric_versions_are_compared_numerically(self):
        versions = parse_versions("Name: Qt_6.9\nName: Qt_6.10\nName: GLIBC_2.9\nName: GLIBC_2.43")
        self.assertEqual(versions, {"Qt": (6, 10), "GLIBC": (2, 43)})

    def test_exported_versions_do_not_become_import_requirements(self):
        output = (
            "Version definition section '.gnu.version_d':\nName: GLIBCXX_3.4.99\n"
            "Version needs section '.gnu.version_r':\nName: GLIBC_2.34\nName: GLIBCXX_3.4.29\n"
        )
        self.assertEqual(
            parse_required_versions(output),
            {"GLIBC": (2, 34), "GLIBCXX": (3, 4, 29)},
        )
        self.assertEqual(parse_required_versions("Name: GLIBC_2.99"), {})

    def test_non_numeric_abi_tags_are_not_version_bounds(self):
        self.assertEqual(parse_versions("Name: GLIBC_PRIVATE\nName: Qt_6_PRIVATE_API"), {})

    def test_published_runtime_requirement_is_rejected_on_noble(self):
        for variant in ("cpu", "cuda", "vulkan"):
            with self.subTest(variant=variant):
                artifact = ElfArtifact(Path(variant) / "libggml-base.so.0", {"GLIBC": (2, 43)}, ())
                with self.assertRaisesRegex(ValueError, r"requires GLIBC_2.43.*provides GLIBC_2.39"):
                    validate_versions([artifact], {"GLIBC": (2, 39)})

    def test_newer_qt_and_cpp_runtime_requirements_are_rejected(self):
        for family, required, available in (
            ("Qt", (6, 10), (6, 4, 2)),
            ("GLIBCXX", (3, 4, 34), (3, 4, 33)),
            ("CXXABI", (1, 3, 16), (1, 3, 15)),
        ):
            with self.subTest(family=family):
                artifact = ElfArtifact(Path("application"), {family: required}, ())
                with self.assertRaises(ValueError):
                    validate_versions([artifact], {family: available})

    def test_compatible_payload_accepts_equal_or_older_requirements(self):
        artifacts = [
            ElfArtifact(Path("app"), {"Qt": (6, 4), "GLIBC": (2, 38)}, ()),
            ElfArtifact(Path("library"), {"GLIBC": (2, 39)}, ()),
        ]
        validate_versions(artifacts, {"Qt": (6, 4, 2), "GLIBC": (2, 39)})

    def test_cuda_driver_and_toolkit_do_not_become_mandatory(self):
        artifact = ElfArtifact(
            Path("libggml-cuda.so"), {},
            ("libcuda.so.1", "libcudart.so.12", "libcublas.so.12", "libcublasLt.so.12"),
        )
        self.assertEqual(
            optional_cuda_shlibs([artifact, artifact]).splitlines(),
            ["libcublas 12 aifilesorter", "libcublasLt 12 aifilesorter",
             "libcuda 1 aifilesorter", "libcudart 12 aifilesorter"],
        )

    def test_standard_system_libraries_are_not_exempted(self):
        artifact = ElfArtifact(
            Path("libggml-cuda.so"), {},
            ("libc.so.6", "libstdc++.so.6", "libgomp.so.1", "libvulkan.so.1"),
        )
        self.assertEqual(optional_cuda_shlibs([artifact]), "")


if __name__ == "__main__":
    unittest.main()
