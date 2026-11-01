"""Offline regression checks for experiment isolation, rendering and grading."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import routing_prompt_replay as replay


class RoutingPromptReplayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.schema = json.dumps({"properties": {"targetFolder": {"enum": ["Docs", "Other", None]}}})
        self.case = {"id": "original-license.txt", "originalFilename": "original-license.txt",
            "recordedFilename": "generated-history.txt", "evidence": "Document summary: Text with $HOME and URLs.",
            "preliminaryLabels": "Legal / Licences", "pluginGuidance": ["Preserve folder IDs."],
            "folders": ["Docs", "Other"], "recordedSystem": "Recorded system.",
            "recordedUser": "Categorize this file.\nFile name: generated-history.txt\nPath: generated-history.txt\n\nContent evidence (primary):\nKeep this literal line:\nFile name: generated-history.txt\n",
            "jsonSchema": self.schema, "maxTokens": 128}

    def test_recorded_baseline_is_byte_exact(self):
        system, user = replay.render(self.case, {"filename": "recorded", "prompt": "recorded"}, self.root)
        self.assertEqual(system, self.case["recordedSystem"])
        self.assertEqual(user, self.case["recordedUser"])

    def test_original_name_changes_metadata_without_rewriting_evidence(self):
        _, user = replay.render(self.case, {"filename": "original", "prompt": "recorded"}, self.root)
        self.assertIn("File name: original-license.txt\nPath: original-license.txt", user)
        self.assertEqual(user.partition("Content evidence (primary)")[2],
                         self.case["recordedUser"].partition("Content evidence (primary)")[2])

    def test_templates_reload_and_preserve_literal_evidence(self):
        (self.root / "system.txt").write_text("Select a folder.")
        (self.root / "user.txt").write_text("${filename}\n${evidence}\n${candidates}\n${plugin_guidance}")
        variant = {"filename": "original", "systemTemplate": "system.txt", "userTemplate": "user.txt"}
        _, user = replay.render(self.case, variant, self.root)
        self.assertIn(self.case["evidence"], user)
        self.assertIn("original-license.txt", user)
        (self.root / "system.txt").write_text("A different experiment.")
        system, _ = replay.render(self.case, variant, self.root)
        self.assertEqual(system, "A different experiment.")
        (self.root / "user.txt").write_text("${expected}")
        with self.assertRaises(KeyError):
            replay.render(self.case, variant, self.root)

    def test_original_name_handles_current_metadata_without_a_duplicate_path(self):
        case = self.case | {"recordedUser": self.case["recordedUser"].replace("Path: generated-history.txt\n", "")}
        _, user = replay.render(case, {"filename": "original", "prompt": "recorded"}, self.root)
        self.assertIn("File name: original-license.txt", user)
        self.assertNotIn("Path:", user)
        self.assertEqual(user.partition("Content evidence (primary)")[2],
                         case["recordedUser"].partition("Content evidence (primary)")[2])

    def test_prepare_balances_variants_and_copies_repeats_exactly(self):
        replay.write_json(self.root / "cases.json", {"cases": [self.case, self.case | {"id": "second.txt"}]})
        replay.write_json(self.root / "variants.json", {"variants": [
            {"id": "recorded", "prompt": "recorded", "filename": "recorded"},
            {"id": "original", "prompt": "recorded", "filename": "original"}],
            "repeatCases": [self.case["id"]]})
        model = self.root / "model.gguf"
        model.write_bytes(b"GGUF")
        with contextlib.redirect_stdout(io.StringIO()):
            replay.prepare(self.root / "cases.json", self.root / "variants.json", self.root / "run",
                           model, "cpu", self.root / "runtime", 0, 42)
        config = replay.read_json(self.root / "run/requests.json")
        requests = config["requests"]
        self.assertEqual([row["variantId"] for row in requests[:4]], ["recorded", "original", "original", "recorded"])
        self.assertEqual(len(requests), 6)
        for row in requests[4:]:
            before = next(item for item in requests if item["id"] == row["repeatOf"])
            for field in ["systemPrompt", "userPrompt", "jsonSchema", "maxTokens"]:
                self.assertEqual(row[field], before[field])
        self.assertEqual(config["sampling"]["seed"], 42)
        self.assertNotIn("expected", config)

    def test_grading_distinguishes_abstentions_errors_and_alternative_folders(self):
        requests, results = [], []
        for index, response in enumerate([
            '{"targetFolder":"Docs","createFolder":false}',
            '{"targetFolder":"Other","createFolder":false}',
            '{"targetFolder":null,"createFolder":false}',
            '{"targetFolder":"Invented","createFolder":false}',
        ]):
            request = {"id": str(index), "caseId": str(index), "variantId": "test", "repeat": False,
                "systemPrompt": "system", "userPrompt": "user", "jsonSchema": self.schema}
            requests.append(request)
            results.append({"id": str(index), "response": response, "elapsedSeconds": 1,
                "systemSha256": replay.digest("system"), "userSha256": replay.digest("user"),
                "schemaSha256": replay.digest(self.schema)})
        results_file = self.root / "results.jsonl"
        results_file.write_text("\n".join(json.dumps(row) for row in results))
        replay.write_json(self.root / "requests.json", {"requests": requests, "outputFile": str(results_file)})
        replay.write_json(self.root / "expected.json", {str(i): "Docs" for i in range(4)})
        with contextlib.redirect_stdout(io.StringIO()):
            summary = replay.score(self.root / "requests.json", self.root / "expected.json")
        counts = summary["variants"]["test"]
        self.assertEqual([counts[key] for key in ["exact", "different", "unresolved", "errors"]], [1, 1, 1, 1])
        self.assertEqual(counts["needsReview"], 3)
        results_file.write_text(results_file.read_text() + "\n" + json.dumps(results[0]))
        with self.assertRaises(ValueError):
            replay.score(self.root / "requests.json", self.root / "expected.json")

    def test_production_prepare_passes_frozen_data_without_rendering_test_prompts(self):
        replay.write_json(self.root / "cases.json", {"cases": [self.case]})
        replay.write_json(self.root / "variants.json", {"variants": [
            {"id": "production", "prompt": "production", "filename": "original"}]})
        profile = self.root / "profile.json"
        profile.write_text('{"id":"frozen-profile"}')
        model = self.root / "model.gguf"
        model.write_bytes(b"GGUF")
        with contextlib.redirect_stdout(io.StringIO()):
            replay.prepare(self.root / "cases.json", self.root / "variants.json", self.root / "run",
                           model, "cpu", self.root / "runtime", 0, 42, [profile])
        config = replay.read_json(self.root / "run/requests.json")
        request = config["requests"][0]
        self.assertNotIn("systemPrompt", request)
        self.assertNotIn("userPrompt", request)
        self.assertNotIn("recordedSystem", request["productionInput"])
        self.assertNotIn("expected", request["productionInput"])
        self.assertEqual(request["productionInput"], {
            "filename": "original-license.txt", "evidence": self.case["evidence"],
            "category": "Legal", "subcategory": "Licences", "folders": self.case["folders"]})
        self.assertEqual(config["productionProfileFiles"], [str(profile.resolve())])


if __name__ == "__main__":
    unittest.main()
