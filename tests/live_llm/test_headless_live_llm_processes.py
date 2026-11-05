"""Check the live runner's real process cleanup without a model or application."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

import headless_live_llm_tests as runner


PROCESS_TIMEOUT_SECONDS = 2
CHILD_MARKER_DELAY_SECONDS = 4
MARKER_GRACE_SECONDS = 1


class LiveProcessTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="aifs-live-process-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_command(self, command):
        return runner.run_command_with_status_progress(
            command=command, env=os.environ.copy(), cwd=self.root,
            timeout_seconds=PROCESS_TIMEOUT_SECONDS, status_path=self.root / "status.json",
            stdout_path=self.root / "stdout.txt", stderr_path=self.root / "stderr.txt",
            case_name="process-cleanup",
        )

    def test_completed_process_preserves_exit_code_and_output(self):
        code, _ = self.run_command([sys.executable, "-c", "print('finished'); raise SystemExit(7)"])
        self.assertEqual(code, 7)
        self.assertEqual((self.root / "stdout.txt").read_text(encoding="utf-8").strip(), "finished")

    def test_timeout_stops_descendant_before_it_can_write(self):
        pid_file = self.root / "child.pid"
        marker = self.root / "orphan-marker.txt"
        child = self.root / "child.py"
        child.write_text(
            "import os,sys,time\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(str(os.getpid()))\n"
            f"time.sleep({CHILD_MARKER_DELAY_SECONDS})\n"
            "Path(sys.argv[2]).write_text('orphan still running')\n",
            encoding="utf-8",
        )
        parent = self.root / "parent.py"
        parent.write_text(
            "import subprocess,sys\n"
            "subprocess.Popen([sys.executable, *sys.argv[1:]]).wait()\n",
            encoding="utf-8",
        )
        try:
            with self.assertRaisesRegex(runner.LiveTestFailure, "timed out"):
                self.run_command([sys.executable, str(parent), str(child), str(pid_file), str(marker)])
            self.assertTrue(pid_file.is_file(), "The descendant must have started before the timeout.")
            time.sleep(CHILD_MARKER_DELAY_SECONDS + MARKER_GRACE_SECONDS)
            self.assertFalse(marker.exists(), "The timed-out descendant continued running.")
        finally:
            if pid_file.exists():
                pid = int(pid_file.read_text())
                if os.name == "nt":
                    subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
                else:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass


if __name__ == "__main__":
    unittest.main()
