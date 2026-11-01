# Frozen folder-routing prompt experiments

This tool compares routing prompts with a real local GGUF model without
reanalyzing documents or images. It freezes the evidence, preliminary labels,
plugin guidance, candidate paths, and native JSON schema from an audited run.
It never moves or renames source files. Reviewed destinations are kept in a
separate grader file and are not available to prompt templates.

The included comparison crosses two prompt styles (recorded and compact) with
two filenames (recorded generated name and original name). Requests rotate
variant order across cases and use a fresh native inference context each time.
Repeated cases check whether the same inputs produce the same response.

## Build once

Use an existing Ninja app build with `compile_commands.json`, clang/GCC, and
the model's backend enabled. The link helper currently supports Linux/macOS;
it does not configure a build or support MSVC link commands.

```sh
cmake --build build --target aifilesorter
python3 tests/live_llm/build_prompt_replay.py \
  --build build --output /tmp/aifs-replay/local-prompt-replay
```

The helper links the normal app objects and native inference implementation.
Static GGML builds avoid dynamically loading a second GGML core. Rebuild the
helper when native app code changes; changing a prompt template does not
require a rebuild.

## Freeze an audited corpus

`analysis.log` must contain the headless JSON progress events and
`inference-audit` must contain the native request audit. The current importer
expects one intact folder-selection request per reviewed file, a common
candidate schema, and filename-only source metadata. It rejects truncated
requests and mismatched or duplicated cases.

```sh
python3 tests/live_llm/routing_prompt_replay.py freeze \
  --run /path/to/audited-analysis \
  --reviewed /path/to/reviewed.json \
  --profile /path/to/johnny-decimal.json \
  --output /tmp/aifs-replay/frozen
```

The profile is the installed plugin's JSON profile. Its ordinary
`prompt_guidance` is preserved; folder-creation guidance is omitted by the
compact existing-folder template. Keep the frozen corpus unchanged when
comparing variants. This tests routing of the recorded evidence, not document
extraction, vision accuracy, or a new end-to-end analysis.

## Prepare, run, score

Copy `prompts/` to an experiment directory and edit its text files and variant
JSON as needed. Templates can use `${filename}`, `${evidence}`,
`${preliminary_labels}`, `${plugin_guidance}`, and `${candidates}`. Replacement
values are inserted literally; dollar signs inside evidence are not expanded.
There is no expected-destination template variable.

```sh
python3 tests/live_llm/routing_prompt_replay.py prepare \
  --cases /tmp/aifs-replay/frozen/cases.json \
  --variants tests/live_llm/prompts/routing_comparison.json \
  --model /path/to/model.gguf --backend metal \
  --runtime "/path/to/AIFileSorter/runtime" \
  --temperature 0 --seed 42 \
  --output /tmp/aifs-replay/run-01
/tmp/aifs-replay/local-prompt-replay /tmp/aifs-replay/run-01/requests.json --validate-only
/tmp/aifs-replay/local-prompt-replay /tmp/aifs-replay/run-01/requests.json
python3 tests/live_llm/routing_prompt_replay.py score \
  --config /tmp/aifs-replay/run-01/requests.json \
  --expected /tmp/aifs-replay/frozen/expected.json
```

Use the real app's runtime directory so its analysis lock prevents concurrent
GUI/headless analysis. Choose an explicit `cpu`, `metal`, `cuda`, or `vulkan`
backend; the runner disables silent CPU fallback. The default preparation uses
2048 context tokens, 128 output tokens from the recorded request, four threads,
and a 60-second per-request deadline. Inspect `requests.json` before running to
change these controls. Temperature zero selects greedy sampling; identical
seeds alone do not guarantee determinism across hardware or builds.

Each prepared config embeds the rendered templates, so later edits cannot
change a run already in progress. Prepare a fresh output directory for each
experiment. The runner refuses to overwrite results. It records input hashes,
native audits, responses, errors, and timings. The grader checks completeness
and input hashes, then writes `summary.json` and `comparison.csv` with exact
matches, different existing destinations, abstentions, errors, and repeat
checks. A different existing destination may still be reasonable; exact-match
grading measures agreement with the reviewed plan.

## Verify production routing

Set a variant's `prompt` to `production` to pass frozen data to the compiled app
code instead of rendering an experimental template:

```json
{"variants":[{"id":"production","prompt":"production","filename":"recorded"}]}
```

Prepare as above, adding `--production-profile /path/to/frozen-plugin-profile.json`
for each profile that was active in the audited run. The copied profile is
research input from that already-authorized run; this does not install or enable
a plugin in the app. The native helper validates it with the production profile
loader, then calls `FolderTreeLLMRouter`, the shared evidence wrapper,
`LocalLLMPromptBuilder`, and `LocalLLMClient::categorize_file_constrained`.

```sh
/tmp/aifs-replay/local-prompt-replay /tmp/aifs-replay/run-01/requests.json --render-production
/tmp/aifs-replay/local-prompt-replay /tmp/aifs-replay/run-01/rendered-requests.json
python3 tests/live_llm/routing_prompt_replay.py score \
  --config /tmp/aifs-replay/run-01/rendered-requests.json \
  --expected /tmp/aifs-replay/frozen/expected.json
```

Rendering performs no model inference. It writes a new `rendered-requests.json`
with the actual production messages and schema. The inference run renders them
again and rejects any change. Native audits can then confirm the exact messages
that reached inference, and grading also checks the production router's accepted
destination. This replay currently requires one existing-folder comparison per
case; it rejects catalogs requiring a tournament or corrective retries. It does
not refresh content extraction or preliminary classification.

Without a `sampling` entry, the native runner uses the client's normal inference
defaults: existing-folder selection at temperature `0`, other text requests at
`0.8`. Explicit replay `sampling.temperature` continues to control the whole
experiment, including production routing. Optional `sampling.existingFolderTemperature`
overrides just the production folder-selection value, for example:

```json
{"sampling":{"temperature":0.8,"existingFolderTemperature":0.2,"seed":42,"threads":4}}
```

For app/headless experiments outside this runner, use
`AI_FILE_SORTER_LLM_EXISTING_FOLDER_TEMPERATURE`; `AI_FILE_SORTER_LLM_TEMPERATURE`
controls other text tasks. Native audits record the effective temperature.
Use a fresh isolated cache for full-analysis experiments; the frozen routing
replay does not consult categorization or routing caches. No persisted app
settings are changed.

Offline checks:

```sh
python3 -m unittest discover -s tests/live_llm -p test_routing_prompt_replay.py -v
```
