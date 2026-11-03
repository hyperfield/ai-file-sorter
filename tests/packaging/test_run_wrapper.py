"""Exercise installed Linux launcher backend selection with package layouts."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[2] / "app" / "scripts"
CPU_LIBRARIES = (
    "libllama.so.0", "libllama-common.so", "libggml.so.0",
    "libggml-base.so.0", "libggml-cpu.so", "libmtmd.so.0",
)
CPU_ARGUMENTS = ("--cuda=off", "--vulkan=off")


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux launcher")
class RunWrapperTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="aifs-launcher-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.app = self.root / "app"
        binary = self.app / "bin" / "aifilesorter-bin"
        binary.parent.mkdir(parents=True)
        binary.write_text(
            '#!/bin/sh\nprintf "%s\\n" "$AI_FILE_SORTER_GPU_BACKEND" '
            '"$AI_FILE_SORTER_GGML_DIR" "$LD_LIBRARY_PATH"\n'
        )
        binary.chmod(0o755)
        tools_dir = self.root / "tools"
        tools_dir.mkdir()
        ldconfig = tools_dir / "ldconfig"
        ldconfig.write_text('#!/bin/sh\nprintf "%s\\n" "$AIFS_TEST_LIBRARIES"\n')
        ldconfig.chmod(0o755)
        plugins = self.root / "qt-plugins"
        plugins.mkdir()
        (plugins / "libqxcb.so").touch()
        self.environment = {
            **os.environ,
            "PATH": str(tools_dir) + os.pathsep + os.environ.get("PATH", os.defpath),
            "QT_QPA_PLATFORM_PLUGIN_PATH": str(plugins),
            "AIFS_TEST_LIBRARIES": "",
        }
        self.wrapper = self.root / "run_aifilesorter.sh"
        subprocess.run([
            sys.executable, str(SCRIPTS / "gen_run_wrapper.py"),
            "--mode", "install", "--install-app-dir", str(self.app),
            "--binary", binary.name, "--template", str(SCRIPTS / "run_aifilesorter.sh.in"),
            "--output", str(self.wrapper),
        ], check=True)

    def runtime(self, directory):
        path = self.app / "lib" / directory
        path.mkdir(parents=True)
        for library in CPU_LIBRARIES:
            (path / library).touch()
        return path

    def launch(self, *arguments, libraries=""):
        result = subprocess.run(
            ["/bin/sh", str(self.wrapper), *arguments],
            env={**self.environment, "AIFS_TEST_LIBRARIES": libraries},
            text=True, capture_output=True, check=True,
        )
        return result.stdout.splitlines()

    def test_packaged_cpu_directory_precedes_legacy_directory(self):
        cpu = self.runtime("precompiled/cpu/bin")
        self.runtime("ggml/wocuda")
        backend, directory, library_path = self.launch(*CPU_ARGUMENTS)
        self.assertEqual((backend, directory), ("cpu", str(cpu)))
        self.assertTrue(library_path.startswith(str(cpu) + ":"))

    def test_legacy_cpu_directory_remains_supported(self):
        cpu = self.runtime("ggml/wocuda")
        backend, directory, _library_path = self.launch(*CPU_ARGUMENTS)
        self.assertEqual((backend, directory), ("cpu", str(cpu)))

    def test_packaged_cuda_precedes_vulkan_when_both_runtimes_are_available(self):
        cpu = self.runtime("precompiled/cpu/bin")
        cuda = self.runtime("precompiled/cuda/bin")
        self.runtime("precompiled/vulkan/bin")
        backend, directory, library_path = self.launch(
            libraries="libcudart.so.12\nlibvulkan.so.1",
        )
        self.assertEqual((backend, directory), ("cuda", str(cuda)))
        self.assertTrue(library_path.startswith(f"{cuda}:{cpu}:"))

    def test_packaged_vulkan_is_selected_when_loader_is_available(self):
        self.runtime("precompiled/cpu/bin")
        vulkan = self.runtime("precompiled/vulkan/bin")
        backend, directory, _library_path = self.launch(
            libraries="libvulkan.so.1",
        )
        self.assertEqual((backend, directory), ("vulkan", str(vulkan)))

    def test_packaged_vulkan_payload_can_supply_cpu_backend(self):
        vulkan = self.runtime("precompiled/vulkan/bin")
        backend, directory, _library_path = self.launch(*CPU_ARGUMENTS)
        self.assertEqual((backend, directory), ("cpu", str(vulkan)))


if __name__ == "__main__":
    unittest.main()
