"""Exercise Windows Vulkan validation before production GGML DLL imports (#125)."""

import argparse
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


PROCESS_TIMEOUT_SECONDS = 30
SUPPRESS_LOADER_DIALOGS = 0x8003  # SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
RUNTIME_DLL_NAMES = {
    "ggml.dll", "ggml-base.dll", "ggml-cpu.dll", "ggml-blas.dll", "ggml-cuda.dll",
    "ggml-vulkan.dll", "llama.dll", "llama-common.dll", "mtmd.dll", "vulkan-1.dll",
}


@unittest.skipUnless(sys.platform == "win32", "Windows DLL startup regression")
class WindowsVulkanStartupTests(unittest.TestCase):
    app_path = None
    launcher_path = None
    incompatible_loader = None

    @classmethod
    def setUpClass(cls):
        if cls.app_path is None or cls.incompatible_loader is None:
            raise unittest.SkipTest("Pass --app and --incompatible-loader to run production startup checks.")
        cls.previous_error_mode = ctypes.windll.kernel32.SetErrorMode(SUPPRESS_LOADER_DIALOGS)
        cls.addClassCleanup(ctypes.windll.kernel32.SetErrorMode, cls.previous_error_mode)
        for name in ("wocuda", "wvulkan"):
            payload = cls.app_path.parent / "lib/ggml" / name
            if not payload.is_dir():
                raise unittest.SkipTest(f"Production runtime payload is unavailable: {payload}")

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="aifs-vulkan-startup-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stage = self.root / "app"
        self.stage.mkdir()
        self.app = self.stage / self.app_path.name
        shutil.copy2(self.app_path, self.app)
        self.launcher = None
        if self.launcher_path is not None:
            self.launcher = self.stage / self.launcher_path.name
            shutil.copy2(self.launcher_path, self.launcher)
        # Copy application dependencies; each case controls the GGML DLL layout.
        for path in self.app_path.parent.glob("*.dll"):
            if path.name.lower() not in RUNTIME_DLL_NAMES:
                shutil.copy2(path, self.stage / path.name)
        for directory in ("platforms", "generic", "imageformats", "styles", "tls"):
            source = self.app_path.parent / directory
            if source.is_dir():
                shutil.copytree(source, self.stage / directory)
        for name in ("wocuda", "wvulkan"):
            relative = Path("lib/ggml") / name
            shutil.copytree(self.app_path.parent / relative, self.stage / relative)
        self.cpu = self.stage / "lib/ggml/wocuda"
        self.vulkan = self.stage / "lib/ggml/wvulkan"
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith(("AI_FILE_SORTER_", "LLAMA_ARG_", "GGML_"))
        }
        self.environment.update({
            "QT_QPA_PLATFORM": "offscreen",
            "QT_FORCE_STDERR_LOGGING": "1",
            "APPDATA": str(self.root / "appdata"),
            "LOCALAPPDATA": str(self.root / "localappdata"),
            "AI_FILE_SORTER_CONFIG_DIR": str(self.root / "config"),
        })

    def launch(self, executable, *arguments, overrides=None):
        return subprocess.run(
            [str(executable), *arguments, "--headless-help"], cwd=self.stage,
            env={**self.environment, **(overrides or {})}, capture_output=True,
            text=True, encoding="utf-8", errors="replace", timeout=PROCESS_TIMEOUT_SECONDS,
            creationflags=subprocess.CREATE_NO_WINDOW, check=False,
        )

    def launch_bootstrapper(self, *arguments):
        if self.launcher is None:
            self.skipTest("This package uses the main executable directly.")
        return self.launch(self.launcher, "--dev-blind-cuda", *arguments)

    def assert_started(self, result, runtime_directory):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(result.stdout.startswith("Usage: aifilesorter "), result.stdout)
        expected = f"Loaded Windows ggml runtime from '{runtime_directory.as_posix()}'"
        self.assertIn(expected, result.stderr.replace("\\", "/"))

    def stage_root_vulkan_payload(self):
        for source in self.vulkan.glob("*.dll"):
            shutil.copy2(source, self.stage / source.name)

    def test_launcher_prefers_validated_loader_to_incompatible_root(self):
        shutil.copy2(self.incompatible_loader, self.stage / "vulkan-1.dll")
        result = self.launch_bootstrapper()
        self.assert_started(result, self.vulkan)
        self.assertIn("Detected compatible bundled Vulkan runtime", result.stderr)
        self.assertIn("Preloaded compatible Vulkan runtime", result.stderr)

    def test_launcher_rejects_incompatible_bundled_loader(self):
        shutil.copy2(self.incompatible_loader, self.vulkan / "vulkan-1.dll")
        for arguments in ((), ("--vulkan=on",)):
            with self.subTest(arguments=arguments):
                result = self.launch_bootstrapper(*arguments)
                self.assert_started(result, self.cpu)
                self.assertIn("missing required export vkGetPhysicalDeviceFeatures2", result.stderr)
                self.assertIn("using CPU backend", result.stderr)

    def test_launcher_cpu_override_ignores_incompatible_root(self):
        shutil.copy2(self.incompatible_loader, self.stage / "vulkan-1.dll")
        result = self.launch_bootstrapper("--vulkan=off")
        self.assert_started(result, self.cpu)

    def test_direct_incompatible_root_loader_falls_back_to_cpu(self):
        self.stage_root_vulkan_payload()
        shutil.copy2(self.incompatible_loader, self.stage / "vulkan-1.dll")
        result = self.launch(self.app, "--allow-direct-launch")
        self.assert_started(result, self.cpu)
        self.assertIn("Vulkan loader is missing vkGetPhysicalDeviceFeatures2", result.stderr)

    def test_direct_compatible_root_loader_starts(self):
        self.stage_root_vulkan_payload()
        result = self.launch(self.app, "--allow-direct-launch")
        self.assert_started(result, self.stage)
        self.assertIn("Preloaded compatible Vulkan runtime", result.stderr)

    def test_direct_missing_validated_loader_falls_back_to_cpu(self):
        result = self.launch(self.app, "--allow-direct-launch", overrides={
            "AI_FILE_SORTER_GPU_BACKEND": "vulkan",
            "LLAMA_ARG_DEVICE": "vulkan",
            "AI_FILE_SORTER_GGML_DIR": str(self.vulkan),
            "AI_FILE_SORTER_VULKAN_RUNTIME_DLL": str(self.root / "missing/vulkan-1.dll"),
        })
        self.assert_started(result, self.cpu)
        self.assertIn("validated Vulkan runtime DLL is no longer present", result.stderr)

    def test_selected_cpu_runtime_cannot_be_shadowed_at_root(self):
        shutil.copy2(self.incompatible_loader, self.stage / "ggml.dll")
        result = self.launch(self.app, "--allow-direct-launch", overrides={
            "AI_FILE_SORTER_GPU_BACKEND": "cpu",
            "AI_FILE_SORTER_GGML_DIR": str(self.cpu),
        })
        self.assert_started(result, self.cpu)

    def test_missing_selected_runtime_reports_a_normal_failure(self):
        result = self.launch(self.app, "--allow-direct-launch", overrides={
            "AI_FILE_SORTER_GPU_BACKEND": "cpu",
            "AI_FILE_SORTER_GGML_DIR": str(self.root / "missing-runtime"),
        })
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("Could not load Windows ggml runtime library", result.stderr)
        self.assertIn("Win32 loader error", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--launcher", type=Path)
    parser.add_argument("--incompatible-loader", required=True, type=Path)
    arguments, test_arguments = parser.parse_known_args()
    WindowsVulkanStartupTests.app_path = arguments.app.resolve()
    WindowsVulkanStartupTests.launcher_path = arguments.launcher.resolve() if arguments.launcher else None
    WindowsVulkanStartupTests.incompatible_loader = arguments.incompatible_loader.resolve()
    unittest.main(argv=[sys.argv[0], *test_arguments])
