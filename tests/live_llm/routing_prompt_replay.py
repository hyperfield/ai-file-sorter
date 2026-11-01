#!/usr/bin/env python3
"""Freeze recorded routing inputs, render file-based variants, and grade native replays."""
import argparse
import bisect
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
from string import Template


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def text(value):
    return value if isinstance(value, str) else value.get("text", "")


def digest(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def freeze(run, reviewed, profile, output):
    output.mkdir(parents=True, exist_ok=False)
    events = []
    for line in (run / "analysis.log").read_text(encoding="utf-8", errors="replace").splitlines():
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        match = re.fullmatch(r"\[SORT\] (.+) \(File\)", value.get("message", ""))
        if match:
            events.append((value["updatedAtUtc"], match[1]))
    events.sort()
    times = [row[0] for row in events]
    expected = {row["fileName"]: row["targetFolder"] for row in read_json(reviewed)["entries"]}
    guidance = read_json(profile)["prompt_guidance"]
    cases = []
    for path in sorted((run / "inference-audit").rglob("attempt-*.json")):
        record = read_json(path)
        if "responseConstraint" not in record:
            continue
        event = bisect.bisect_right(times, record["createdAtUtc"]) - 1
        if event < 0:
            raise ValueError("Routing audit has no preceding SORT event")
        original_name = events[event][1]
        prompt = text(record["originalUserPrompt"])
        name = re.search(r"^File name: (.+)$", prompt, re.MULTILINE)
        labels = re.search(r"^(?:- )?Preliminary labels \((?:not a required destination|optional clues)\): (.+)\.$", prompt, re.MULTILINE)
        evidence = re.search(r"((?:Document summary|Image description): [\s\S]*?)\n\[\[AI_FILE_SORTER_CONTENT_EVIDENCE_END\]\]", prompt)
        schema = text(record["responseConstraint"]["jsonSchema"])
        permitted = json.loads(schema)["properties"]["targetFolder"]["enum"]
        folders = [value for value in permitted if isinstance(value, str)]
        if not name or not labels or not all(line in prompt for line in guidance):
            raise ValueError(f"Missing filename, labels or plugin guidance: {path}")
        if any(value.startswith("/") or ".." in value.split("/") for value in folders):
            raise ValueError("Unsafe candidate path")
        if json.loads(schema)["properties"]["createFolder"] != {"const": False}:
            raise ValueError("Replay requires existing-folder-only schemas")
        fit = record["fitAttempts"][record["selectedFitAttempt"]]
        if fit["userPromptChanged"] or not record["allPromptTokensAcceptedExactly"]:
            raise ValueError("Only intact, fully accepted prompts can be frozen")
        cases.append({
            "id": original_name, "originalFilename": original_name, "recordedFilename": name[1],
            "evidence": evidence[1] if evidence else "", "preliminaryLabels": labels[1],
            "pluginGuidance": guidance, "folders": folders,
            "recordedSystem": text(record["systemPrompt"]), "recordedUser": prompt,
            "jsonSchema": schema, "maxTokens": record["runtime"]["generationLimit"],
            "auditFile": str(path), "auditSha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "createdAtUtc": record["createdAtUtc"],
        })
    cases.sort(key=lambda case: case["createdAtUtc"])
    if len({case["id"] for case in cases}) != len(cases) or {case["id"] for case in cases} != set(expected):
        raise ValueError("Frozen audits must match the reviewed case set exactly")
    if len({digest(case["jsonSchema"]) for case in cases}) != 1:
        raise ValueError("Candidate/schema differences need a separate experiment")
    write_json(output / "cases.json", {"version": 1, "cases": cases})
    # The grader's answers are kept separate and never rendered into model requests.
    write_json(output / "expected.json", expected)
    print(f"Frozen {len(cases)} cases; expected destinations are stored separately.")


def replace_metadata_filename(prompt, recorded, original):
    boundary = prompt.find("Content evidence (primary")
    if boundary < 0:
        boundary = prompt.find("[[AI_FILE_SORTER_FOLDER_TREE_MODE]]")
    if boundary < 0:
        raise ValueError("Cannot identify the metadata/content boundary")
    head, tail = prompt[:boundary], prompt[boundary:]
    pattern = re.compile(r"^(File name: |Path: |Full path: )" + re.escape(recorded) + r"$", re.MULTILINE)
    replaced, count = pattern.subn(lambda match: match[1] + original, head)
    if count not in (1, 2) or len(re.findall(r"^File name: ", head, re.MULTILINE)) != 1:
        raise ValueError("Expected one filename and at most one filename-only path")
    return replaced + tail


def render(case, variant, template_root):
    mode = variant["filename"]
    if mode not in ("recorded", "original"):
        raise ValueError("filename must be recorded or original")
    filename = case["originalFilename"] if mode == "original" else case["recordedFilename"]
    if variant.get("prompt") == "recorded":
        user = case["recordedUser"]
        if mode == "original":
            user = replace_metadata_filename(user, case["recordedFilename"], filename)
        return case["recordedSystem"], user
    fields = {
        "filename": filename, "evidence": case["evidence"],
        "preliminary_labels": case["preliminaryLabels"],
        "plugin_guidance": "\n".join("- " + line for line in case["pluginGuidance"]),
        "candidates": "\n".join(case["folders"]),
    }
    def load(key):
        return Template((template_root / variant[key]).read_text(encoding="utf-8")).substitute(fields).strip()
    return load("systemTemplate"), load("userTemplate")


def prepare(cases_path, variants_path, output, model, backend, runtime, temperature, seed, production_profiles=()):
    if not 0 <= temperature <= 10 or not 0 <= seed < 2**32 - 1:
        raise ValueError("Use a finite temperature and a fixed uint32 seed")
    output.mkdir(parents=True, exist_ok=False)
    cases = read_json(cases_path)["cases"]
    experiment = read_json(variants_path)
    variants = experiment["variants"]
    if (not variants or any(not isinstance(variant.get("id"), str) or not variant["id"] for variant in variants)
            or len({variant["id"] for variant in variants}) != len(variants)):
        raise ValueError("Variant ids must be nonempty and unique")
    requests = []
    for index, case in enumerate(cases):
        # Rotate variant order across cases; every request uses a fresh native context.
        ordered = variants[index % len(variants):] + variants[:index % len(variants)]
        for variant in ordered:
            request = {"id": f"{len(requests):04d}", "caseId": case["id"],
                "variantId": variant["id"], "repeat": False,
                "jsonSchema": case["jsonSchema"], "maxTokens": case["maxTokens"]}
            if variant.get("prompt") == "production":
                if variant["filename"] not in ("recorded", "original"):
                    raise ValueError("filename must be recorded or original")
                category, _, subcategory = case["preliminaryLabels"].partition(" / ")
                request["productionInput"] = {
                    "filename": case["originalFilename"] if variant["filename"] == "original" else case["recordedFilename"],
                    "evidence": case["evidence"], "category": category, "subcategory": subcategory,
                    "folders": case["folders"]}
            else:
                system, user = render(case, variant, variants_path.parent)
                request.update(systemPrompt=system, userPrompt=user)
            requests.append(request)
    repeats = set(experiment.get("repeatCases", []))
    if not repeats.issubset({case["id"] for case in cases}):
        raise ValueError("Repeat case does not exist")
    for request in list(requests):
        if request["caseId"] in repeats:
            requests.append(request | {"id": f"{len(requests):04d}", "repeat": True, "repeatOf": request["id"]})
    config = {"version": 1, "modelPath": str(model.resolve()), "backend": backend,
        "runtimeDirectory": str(runtime.resolve()), "contextTokens": 2048, "timeoutSeconds": 60,
        "auditDirectory": str(output.resolve() / "audit"), "outputFile": str(output.resolve() / "results.jsonl"),
        "sampling": {"temperature": temperature, "seed": seed, "minP": 0.05, "threads": 4, "batchTokens": 2048},
        "requests": requests}
    if production_profiles:
        config["productionProfileFiles"] = [str(path.resolve()) for path in production_profiles]
    write_json(output / "requests.json", config)
    write_json(output / "provenance.json", {"casesSha256": hashlib.sha256(cases_path.read_bytes()).hexdigest(),
        "variantsSha256": hashlib.sha256(variants_path.read_bytes()).hexdigest(),
        "casesFile": str(cases_path.resolve()), "variantsFile": str(variants_path.resolve()),
        "modelBytes": model.stat().st_size, "modelModifiedNs": model.stat().st_mtime_ns,
        "productionProfiles": [{"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for path in production_profiles]})
    print(f"Prepared {len(requests)} requests ({len(cases)} cases x {len(variants)} variants, plus repeat checks).")


def score(config_path, expected_path):
    config = read_json(config_path)
    expected = read_json(expected_path)
    requests = {row["id"]: row for row in config["requests"]}
    results = [json.loads(line) for line in Path(config["outputFile"]).read_text().splitlines() if line.strip()]
    if len(results) != len(requests) or {row["id"] for row in results} != set(requests):
        raise ValueError("Replay results are incomplete or contain duplicate/unexpected ids")
    by_id = {row["id"]: row for row in results}
    totals, rows, repeat_checks = {}, [], []
    for result in results:
        request = requests[result["id"]]
        for field in ["system", "user", "schema"]:
            key = {"system": "systemPrompt", "user": "userPrompt", "schema": "jsonSchema"}[field]
            if result[field + "Sha256"] != digest(request[key]):
                raise ValueError("Input digest mismatch")
        if request["repeat"]:
            original = by_id[request["repeatOf"]]
            try:
                same_response = json.loads(result.get("response", "")) == json.loads(original.get("response", ""))
            except (ValueError, TypeError):
                same_response = False
            repeat_checks.append({"variant": request["variantId"], "case": request["caseId"],
                "sameResponse": same_response,
                "error": result.get("error", original.get("error", ""))})
            continue
        counter = totals.setdefault(request["variantId"], {"cases": 0, "exact": 0, "different": 0, "unresolved": 0, "errors": 0})
        counter["cases"] += 1
        target = None
        outcome = "errors"
        try:
            if result.get("error"):
                raise ValueError(result["error"])
            response = json.loads(result["response"])
            target = response["targetFolder"]
            permitted = json.loads(request["jsonSchema"])["properties"]["targetFolder"]["enum"]
            if set(response) != {"targetFolder", "createFolder"} or response["createFolder"] is not False or target not in permitted:
                raise ValueError("Invalid destination response")
            if "productionInput" in request and (result.get("productionRoutingCalls") != 1 or result.get("productionTarget") != target):
                raise ValueError("Production router outcome differs from the model response")
            outcome = "unresolved" if target is None else "exact" if target == expected[request["caseId"]] else "different"
        except (KeyError, ValueError, TypeError):
            pass
        counter[outcome] += 1
        rows.append({"case": request["caseId"], "variant": request["variantId"], "expected": expected[request["caseId"]],
            "target": target, "outcome": outcome, "seconds": result["elapsedSeconds"], "error": result.get("error", "")})
    for variant, count in totals.items():
        count["needsReview"] = count["cases"] - count["exact"]
        count["medianSeconds"] = statistics.median(row["seconds"] for row in rows if row["variant"] == variant)
    summary = {"variants": totals, "repeatChecks": repeat_checks, "rows": rows}
    destination = config_path.parent
    write_json(destination / "summary.json", summary)
    with (destination / "comparison.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    print(json.dumps({"variants": totals, "repeatChecks": repeat_checks}, indent=2))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    freeze_args = commands.add_parser("freeze")
    for name in ["run", "reviewed", "profile", "output"]:
        freeze_args.add_argument("--" + name, type=Path, required=True)
    prepare_args = commands.add_parser("prepare")
    for name in ["cases", "variants", "output", "model", "runtime"]:
        prepare_args.add_argument("--" + name, type=Path, required=True)
    prepare_args.add_argument("--backend", choices=["cpu", "metal", "cuda", "vulkan"], required=True)
    prepare_args.add_argument("--temperature", type=float, default=0.0)
    prepare_args.add_argument("--seed", type=int, default=42)
    prepare_args.add_argument("--production-profile", type=Path, action="append", default=[])
    score_args = commands.add_parser("score")
    score_args.add_argument("--config", type=Path, required=True)
    score_args.add_argument("--expected", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "freeze":
        freeze(args.run, args.reviewed, args.profile, args.output)
    elif args.command == "prepare":
        prepare(args.cases, args.variants, args.output, args.model, args.backend, args.runtime,
                args.temperature, args.seed, args.production_profile)
    else:
        score(args.config, args.expected)


if __name__ == "__main__":
    main()
