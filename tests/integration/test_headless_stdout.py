"""Check the production headless executable's stdout/stderr contract without an LLM."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


PROCESS_TIMEOUT_SECONDS = 20


class HeadlessOutputTests(unittest.TestCase):
    app_path = None
    check_macos_pdfium = False

    def setUp(self):
        if self.app_path is None:
            self.skipTest("Pass --app /path/to/aifilesorter to run production CLI checks.")
        self.assertTrue(self.app_path.is_file(), self.app_path)
        temporary = tempfile.TemporaryDirectory(prefix="aifs-headless-output-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.status = self.root / "status.json"
        config = self.root / "config" / "AIFileSorter" / "config.ini"
        config.parent.mkdir(parents=True)
        config.write_text("[Settings]\nLanguage=English\n", encoding="utf-8")
        self.environment = {
            **os.environ,
            "AI_FILE_SORTER_CONFIG_DIR": str(self.root / "config"),
            "XDG_CACHE_HOME": str(self.root / "cache"),
            "APPDATA": str(self.root / "appdata"),
            "QT_QPA_PLATFORM": "offscreen",
        }
        if sys.platform == "darwin":
            self.environment = {key: value for key, value in self.environment.items()
                                if not key.startswith("DYLD_")}

    def launch(self, *arguments, environment=None, app_path=None):
        return subprocess.run(
            [str(self.app_path if app_path is None else app_path), *arguments],
            cwd=self.root,
            env=self.environment if environment is None else environment,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="strict",
            timeout=PROCESS_TIMEOUT_SECONDS,
            check=False,
        )

    def assert_status_stream(self, result, state):
        lines = [line for line in result.stdout.splitlines() if line.strip()]
        self.assertTrue(lines, result.stderr)
        records = []
        for line in lines:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                self.fail(f"Non-JSON stdout line: {line!r}")
            self.assertIsInstance(record, dict)
            self.assertEqual(record.get("schemaVersion"), 1)
            records.append(record)
        self.assertEqual(records[-1]["status"], state)
        self.assertEqual(records[-1], json.loads(self.status.read_text(encoding="utf-8")))
        # Both default spdlog startup output and named application loggers must use stderr.
        self.assertIn("Loggers initialized.", result.stderr)
        self.assertIn("Loaded settings", result.stderr)

    def test_review_only_stdout_is_json_and_diagnostics_use_stderr(self):
        for extra_arguments in ((), ("--console-log", "--development")):
            with self.subTest(extra_arguments=extra_arguments):
                result = self.launch(
                    "--headless", "--operation", "categorize", "--path", str(self.source),
                    "--review-only", "--status-file", str(self.status), *extra_arguments,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assert_status_stream(result, "completed")
                self.assertFalse(list(self.source.iterdir()))

    def test_legacy_explorer_fallback_keeps_stdout_json(self):
        sidecar = self.root / "explorer-104.settings.json"
        config = self.root / "config" / "AIFileSorter" / "config.ini"
        before = config.read_bytes()
        cases = (
            ("explorer-104", {"useSubcategories": True}, True, True),
            ("explorer-110", {"sortingMode": "ExistingFolderTree"}, True, False),
            ("headless-other", {"useSubcategories": True}, True, False),
            ("explorer-no-sidecar", {}, False, False),
        )
        for job_id, settings, use_sidecar, legacy in cases:
            with self.subTest(job_id=job_id):
                sidecar.write_text(json.dumps(settings), encoding="utf-8")
                arguments = [
                    "--headless", "--operation", "categorize", "--path", str(self.source),
                    "--review-only", "--job-id", job_id, "--status-file", str(self.status),
                ]
                if use_sidecar:
                    arguments.extend(("--settings-overrides-file", str(sidecar)))
                result = self.launch(*arguments)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assert_status_stream(result, "completed")
                self.assertEqual("legacy Explorer job" in result.stderr, legacy)
                self.assertEqual(config.read_bytes(), before)

    def test_analysis_failure_preserves_json_stdout(self):
        first = self.source / "first.txt"
        second = self.root / "second.txt"
        first.write_text("First fixture", encoding="utf-8")
        second.write_text("Second fixture", encoding="utf-8")
        result = self.launch(
            "--headless", "--operation", "categorize", "--path", str(first), "--path", str(second),
            "--review-only", "--status-file", str(self.status),
        )
        self.assertEqual(result.returncode, 4, result.stderr)
        self.assert_status_stream(result, "failed")

    def test_saved_plan_apply_stdout_is_json(self):
        source = self.source / "original.txt"
        source.write_text("Fixture contents", encoding="utf-8")
        plan = self.root / "review.json"
        plan.write_text(json.dumps({
            "schemaVersion": 1,
            "kind": "aifs.headlessReviewPlan",
            "operation": "rename",
            "paths": [str(source)],
            "applyOptions": {
                "baseDir": str(self.source),
                "undoDir": str(self.root / "undo"),
                "applySuggestedNames": True,
                "moveCategorizedEntries": False,
            },
            "entries": [{
                "filePath": str(self.source),
                "fileName": source.name,
                "type": "file",
                "suggestedName": "renamed.txt",
                "renameOnly": True,
            }],
        }), encoding="utf-8")
        result = self.launch(
            "--headless-apply", "--review-file", str(plan), "--status-file", str(self.status),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_status_stream(result, "completed")
        self.assertFalse(source.exists())
        self.assertEqual((self.source / "renamed.txt").read_text(encoding="utf-8"), "Fixture contents")

    def test_help_is_usage_text_without_log_lines(self):
        result = self.launch("--headless-help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(result.stdout.startswith("Usage: aifilesorter "), result.stdout)
        self.assertNotIn("Loggers initialized.", result.stdout)
        self.assertIn("Loggers initialized.", result.stderr)

    def test_macos_embedded_pdfium_loads_beside_relocated_app(self):
        if not self.check_macos_pdfium:
            self.skipTest("Requires a macOS embedded-PDFium build.")
        relocated = (self.root / "relocated app 東京").resolve()
        relocated.mkdir()
        executable = relocated / self.app_path.name
        runtime = relocated / "libpdfium.dylib"
        shutil.copy2(self.app_path, executable)
        shutil.copy2(self.app_path.parent / runtime.name, runtime)
        result = self.launch(
            "--headless", "--operation", "categorize", "--path", str(self.source),
            "--review-only", "--status-file", str(self.status),
            app_path=executable, environment={**self.environment, "DYLD_PRINT_LIBRARIES": "1"},
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_status_stream(result, "completed")
        self.assertIn(str(runtime), result.stderr, "dyld must load the adjacent PDFium copy")
        self.assertFalse(list(self.source.iterdir()))

    def test_invalid_arguments_use_stderr(self):
        result = self.launch("--headless", "--operation", "invalid", "--path", str(self.source))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("Unsupported headless operation", result.stderr)
        self.assertIn("Usage: aifilesorter ", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path, help="Built production executable.")
    parser.add_argument("--check-macos-pdfium", action="store_true",
                        help="Check adjacent embedded PDFium loading after relocating the macOS app.")
    arguments, test_arguments = parser.parse_known_args()
    HeadlessOutputTests.app_path = arguments.app.resolve()
    HeadlessOutputTests.check_macos_pdfium = arguments.check_macos_pdfium
    unittest.main(argv=[sys.argv[0], *test_arguments])
