#!/usr/bin/env python3
"""Exercise real Makefile incremental rebuilds without installed app libraries."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

MAKEFILE = Path(__file__).resolve().parents[1] / "app/Makefile"
MAKE = shutil.which("gmake") or shutil.which("make")


@unittest.skipUnless(MAKE and os.name == "posix", "requires POSIX make")
class MakeBuildConfigTests(unittest.TestCase):
    def test_dependency_identity_changes_rebuild_but_identical_builds_do_not(self):
        with tempfile.TemporaryDirectory(prefix="aifs-make-config-") as tmp:
            root = Path(tmp)
            for version in ["qt-old", "qt-new"]:
                (root / version / "lib").mkdir(parents=True)
            (root / "qt-current").symlink_to(root / "qt-old", target_is_directory=True)
            (root / "qt-version").write_text("6.9.0")
            (root / "compiler-version").write_text("fixture compiler 1")
            compiler = root / "compiler"
            compiler.write_text('''#!/usr/bin/env python3
import os, pathlib, sys
root = pathlib.Path(os.environ["AIFS_MAKE_FIXTURE"])
if "--version" in sys.argv:
    print((root / "compiler-version").read_text())
else:
    with (root / "compile-count").open("a") as f: f.write("compiled\\n")
    pathlib.Path(sys.argv[sys.argv.index("-o") + 1]).write_text("object")
''')
            pkg = root / "pkg-config"
            pkg.write_text('''#!/usr/bin/env python3
import os, pathlib, sys
root = pathlib.Path(os.environ["AIFS_MAKE_FIXTURE"])
if "--modversion" in sys.argv: print((root / "qt-version").read_text())
elif "--variable=libdir" in sys.argv: print(root / "qt-current/lib")
elif "--cflags" in sys.argv and "Qt6Core" in sys.argv: print("-I" + str(root / "qt-current/include"))
elif "--libs" in sys.argv and "Qt6Core" in sys.argv: print("-lQtCore")
''')
            compiler.chmod(0o755)
            pkg.chmod(0o755)
            (root / "Makefile").write_text(
                f"include {MAKEFILE}\n"
                ".PHONY: probe\n"
                "probe: $(OBJ_DIR)/probe.o\n"
                "$(OBJ_DIR)/probe.o: probe.cpp $(BUILD_CONFIG_STAMP)\n"
                "\t$(CXX) $(CXXFLAGS) -c $< -o $@\n")
            (root / "probe.cpp").write_text("int probe;\n")
            env = {**os.environ, "AIFS_MAKE_FIXTURE": str(root)}

            def build(flags="-DMODE=1", ldflags=""):
                result = subprocess.run(
                    [MAKE, "--no-print-directory", "UNAME=Linux", "OBJ_DIR=obj", "probe",
                     f"CXX={compiler}", f"PKG_CONFIG={pkg}", f"CXXFLAGS={flags}", f"LDFLAGS={ldflags}",
                     "MEDIAINFO_REQUIRE=0", "RCC=true", "LRELEASE=true"],
                    cwd=root, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                self.assertEqual(result.returncode, 0, result.stdout)
                return len((root / "compile-count").read_text().splitlines())

            self.assertEqual(build(), 1)
            stamp = root / "obj/.build-config"
            initial_stamp_time = stamp.stat().st_mtime_ns
            self.assertEqual(build(), 1)
            self.assertEqual(stamp.stat().st_mtime_ns, initial_stamp_time)
            # Cross whole seconds too, for older make implementations/filesystems.
            time.sleep(1.05)
            self.assertEqual(build("-DMODE=2"), 2, "compile-flag changes must invalidate objects")
            time.sleep(1.05)
            (root / "qt-version").write_text("6.12.0")
            self.assertEqual(build("-DMODE=2"), 3, "Qt upgrades must invalidate objects")
            time.sleep(1.05)
            (root / "qt-current").unlink()
            (root / "qt-current").symlink_to(root / "qt-new", target_is_directory=True)
            self.assertEqual(build("-DMODE=2"), 4, "same-version package replacements must invalidate objects")
            time.sleep(1.05)
            (root / "compiler-version").write_text("fixture compiler 2")
            self.assertEqual(build("-DMODE=2"), 5, "compiler upgrades must invalidate objects")
            time.sleep(1.05)
            self.assertEqual(build("-DMODE=2", "-Lnew-library-path"), 6, "link settings must be refreshed")
            final_stamp_time = stamp.stat().st_mtime_ns
            self.assertEqual(build("-DMODE=2", "-Lnew-library-path"), 6)
            self.assertEqual(stamp.stat().st_mtime_ns, final_stamp_time)


if __name__ == "__main__":
    unittest.main()
