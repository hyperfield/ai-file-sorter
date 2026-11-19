#!/usr/bin/env python3
"""Native Mach-O regressions for recursive macOS runtime staging and relocation."""

import hashlib
import importlib.util
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "app/scripts/macos_bundle_runtime.py"
spec = importlib.util.spec_from_file_location("macos_bundle_runtime", SCRIPT)
runtime = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runtime
spec.loader.exec_module(runtime)


@unittest.skipUnless(sys.platform == "darwin", "requires native macOS Mach-O tools")
class MacOSBundleRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="aifs-bundle-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "developer libraries 東京"
        self.source.mkdir()
        self.bundle = self.root / "relocated app été/Fixture.app"
        self.executable = self.bundle / "Contents/MacOS/fixture"
        self.executable.parent.mkdir(parents=True)
        (self.bundle / "Contents/Frameworks").mkdir()
        self.leaf = self.compile("leaf", "int leaf(void) { return 42; }", dylib=True)
        self.curl = self.compile("curl", "extern int leaf(void); int fixture(void) { return leaf(); }",
                                 self.leaf, dylib=True)
        executable = self.compile("main", '#include <curl/curl.h>\n#include <stdio.h>\n'
                                  'extern int fixture(void); int main(void) {\n'
                                  'puts(curl_version()); return fixture() == 42 ? 0 : 1; }',
                                  self.curl, "-lcurl", "-Wl,-rpath," + str(self.source))
        shutil.copy2(executable, self.executable)
        self.closure = runtime.DependencyClosure(self.bundle, self.executable, "precompiled")

    def compile(self, name, source, *libraries, dylib=False):
        code = self.source / (name + ".c")
        code.write_text(source)
        output = self.source / (f"lib{name}.4.dylib" if dylib else name)
        command = ["clang", str(code), "-Wl,-headerpad_max_install_names", "-o", str(output)]
        if dylib:
            command += ["-dynamiclib", "-Wl,-install_name," + str(output)]
        subprocess.run([*command, *map(str, libraries)], check=True, capture_output=True)
        return output

    def test_recursive_closure_preserves_system_curl_and_runs_after_relocation(self):
        originals = {path: hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in (self.curl, self.leaf)}
        self.leaf.chmod(0o444)
        self.closure.collect()
        self.closure.rewrite(platform.machine())
        self.closure.audit()
        image = runtime.inspect(self.executable)
        self.assertIn("/usr/lib/libcurl.4.dylib", image.dependencies)
        self.assertIn("@rpath/libcurl.4.dylib", image.dependencies)
        self.assertEqual(len(image.dependencies), len(set(image.dependencies)))
        self.assertTrue(all(value.startswith("@loader_path/") for value in image.rpaths))
        self.assertEqual(len(self.closure.images), 3)
        for path, digest in originals.items():
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)
        self.source.rename(self.root / "hidden developer files")
        for path in self.closure.images:
            subprocess.run(["codesign", "--force", "--sign", "-", str(path)],
                           check=True, capture_output=True)
        # This also checks actual dyld-loaded paths with all inherited DYLD_*
        # overrides removed and an unrelated working directory.
        runtime.verify_loaded(self.bundle, self.executable)

    def test_missing_dependency_reports_the_exact_loader_and_path(self):
        self.leaf.unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing runtime dependency:.*libcurl.*libleaf"):
            self.closure.collect()

    def test_original_rpath_takes_precedence_over_a_stale_staged_library(self):
        subprocess.run(["install_name_tool", "-change", str(self.curl),
                        "@rpath/libcurl.4.dylib", str(self.executable)], check=True, capture_output=True)
        stale = self.compile("stale", "int fixture(void) { return 7; }", dylib=True)
        self.closure.runtime.mkdir(parents=True)
        shutil.copy2(stale, self.closure.runtime / "libcurl.4.dylib")
        self.closure.collect()
        target = self.closure.edges[self.executable.resolve(), "@rpath/libcurl.4.dylib"]
        self.assertEqual(target, (self.closure.frameworks / "libcurl.4.dylib").resolve())
        self.assertEqual(target.read_bytes(), self.curl.read_bytes())

    def test_loader_relative_dependency_uses_the_original_executable_directory(self):
        subprocess.run(["install_name_tool", "-change", str(self.curl),
                        "@loader_path/libcurl.4.dylib", str(self.executable)],
                       check=True, capture_output=True)
        closure = runtime.DependencyClosure(self.bundle, self.executable, "precompiled",
                                            self.source / "main")
        closure.collect()
        target = closure.edges[self.executable.resolve(), "@loader_path/libcurl.4.dylib"]
        self.assertEqual(target.read_bytes(), self.curl.read_bytes())

    def test_symlink_cannot_mutate_an_external_image(self):
        (self.bundle / "Contents/Frameworks/escape.dylib").symlink_to(self.curl)
        original = self.curl.read_bytes()
        with self.assertRaisesRegex(RuntimeError, "symlink escapes"):
            self.closure.collect()
        self.assertEqual(self.curl.read_bytes(), original)

    def test_architecture_mismatch_is_a_clear_error(self):
        self.closure.collect()
        other = "x86_64" if platform.machine() == "arm64" else "arm64"
        with self.assertRaisesRegex(RuntimeError, "Architecture mismatch"):
            self.closure.rewrite(other)


if __name__ == "__main__":
    unittest.main()
