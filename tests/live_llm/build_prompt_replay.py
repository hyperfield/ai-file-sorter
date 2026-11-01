#!/usr/bin/env python3
"""Link the prompt replay tool against an existing Ninja app build (clang/GCC)."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1] / "tools/local_prompt_replay_main.cpp"
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in commands if row["file"].endswith("/main.cpp")
                 and "CMakeFiles/aifilesorter.dir/" in row.get("command", ""))
    compile_args = shlex.split(entry["command"])
    original_object = compile_args[compile_args.index("-o") + 1]
    compile_args[compile_args.index("-o") + 1] = str(output.with_suffix(".o"))
    compile_args[-1] = str(source)
    cache = (build / "CMakeCache.txt").read_text()
    if "GGML_BACKEND_DL:BOOL=OFF" in cache:
        compile_args.insert(1, "-DAIFS_REPLAY_STATIC_GGML=1")
    # Original main.cpp's dependency-file target is harmless, but keep its file separate.
    if "-MF" in compile_args:
        compile_args[compile_args.index("-MF") + 1] = str(output.with_suffix(".d"))
    subprocess.run(compile_args, cwd=build, check=True)
    commands = subprocess.check_output(["ninja", "-t", "commands", "aifilesorter"], cwd=build, text=True)
    link_args = shlex.split(commands.splitlines()[-1].split(" && ")[1])
    link_args[link_args.index(original_object)] = str(output.with_suffix(".o"))
    link_args[link_args.index("-o") + 1] = str(output)
    subprocess.run(link_args, cwd=build, check=True)
    print(output)


if __name__ == "__main__":
    main()
