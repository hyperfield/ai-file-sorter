# Windows CUDA Detection And Packaging Fix Plan

## Summary

Root cause: Windows CUDA support currently depends on a globally discoverable Toolkit-style `cudart64_*.dll`, while the app can ship `ggml-cuda.dll` without the CUDA runtime dependency set it needs. The launcher and in-app benchmark also use different CUDA detection paths, and the Compatibility Benchmark stores stale results in `config.ini`, so users can see old “CUDA unavailable” outcomes after fixing their driver/toolkit.

Research basis: local probe and launcher code in [WindowsCudaProbe.cpp](/C:/Users/jland/Documents/Codex/AI-File-Sorter/app/lib/WindowsCudaProbe.cpp:160), [startapp_windows.cpp](/C:/Users/jland/Documents/Codex/AI-File-Sorter/app/startapp_windows.cpp:211), benchmark persistence in [Settings.cpp](/C:/Users/jland/Documents/Codex/AI-File-Sorter/app/lib/Settings.cpp:364), Windows staging in [build_llama_windows.ps1](/C:/Users/jland/Documents/Codex/AI-File-Sorter/app/scripts/build_llama_windows.ps1:741), NVIDIA redistribution allowance in [CUDA Toolkit EULA Attachment A](https://docs.nvidia.com/cuda/archive/13.0.3/eula/index.html#attachment-a), Toolkit path behavior in [CUDA Windows install guide](https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html), and version context in [CUDA 13.2 release notes](https://docs.nvidia.com/cuda/archive/13.2.0/cuda-toolkit-release-notes/index.html).

## Interface Changes

- [x] Extend `WindowsCudaProbe` with a non-breaking `ProbeOptions` overload: `ggml_directory`, `preferred_runtime_directories`, and `include_system_directories`.
- [x] Extend `ProbeResult` with `runtime_source` and enough detail to distinguish `driver_missing`, `driver_no_device`, `runtime_missing`, `runtime_unusable`, and `backend_dependency_mismatch`.
- [x] Add Windows CUDA payload helpers to `GgmlRuntimePaths`: candidate dirs and resolver for packaged `lib/ggml/wcuda` and `lib/precompiled/cuda/bin`.
- [x] Add benchmark state fields to `Settings`: `BenchmarkProbeSignature` and `BenchmarkProbeSchemaVersion`.

## Execution Steps

- [x] 1. Start from a clean branch off `upstream/dev`: `fix/windows-cuda-runtime-detection`.
- [x] 2. Confirm remotes: `origin` points to `JLanders96/ai-file-sorter`, `upstream` points to `hyperfield/ai-file-sorter`, and push/PR work remains in the contribution steps.
- [ ] 3. Capture sanitized baseline evidence: current branch, `CUDA_PATH`, `nvidia-smi`, `nvcc --version`, and current portable runtime directory listing.
- [x] 4. Add `windows_cuda_payload_candidate_dirs(exe_path)` to `GgmlRuntimePaths`.
- [x] 5. Return candidates in this order: `lib/ggml/wcuda`, `ggml/wcuda`, `lib/precompiled/cuda/bin`, then app `bin`/exe dir if present.
- [x] 6. Add `resolve_windows_cuda_payload_dir(exe_path)` requiring `llama.dll`, `ggml.dll`, `ggml-cuda.dll`, and all staged CUDA redistributable dependency families: `cudart64_*.dll`, `cublas64_*.dll`, and `cublasLt64_*.dll`.
- [x] 7. Add `WindowsCudaProbe::ProbeOptions` while keeping the existing `probe(optional<path>)` wrapper.
- [x] 8. Add `RuntimeDirectorySource::Packaged` and rank it before Toolkit hints and `PATH`.
- [x] 9. Make candidate runtime directories merge packaged dirs, `CUDA_PATH`, `CUDA_PATH_V*`, standard Toolkit roots, then `PATH`.
- [x] 10. Keep Toolkit `bin/x64` preferred over Toolkit `bin`/root and keep PhysX ranked last.
- [x] 11. Pass all relevant DLL dirs into `AddDllDirectory` before loading `ggml-cuda.dll`.
- [x] 12. Keep `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)` for runtime DLLs.
- [x] 12a. Load `nvcuda.dll` from System32 with an explicit safe search path now that the launcher uses the shared probe for driver detection.
- [x] 13. Update `Utils::is_cuda_available()` to resolve packaged CUDA dirs even when `AI_FILE_SORTER_GGML_DIR` is absent, covering MSIX/App Store builds.
- [x] 14. Update `Utils::query_cuda_memory()` and `best_runtime_library_path()` calls to use the same `ProbeOptions`.
- [x] 15. Replace the duplicate launcher CUDA scanner in `startapp_windows.cpp` with `WindowsCudaProbe::probe(options)`.
- [x] 15a. Make launcher CUDA payload resolution use `GgmlRuntimePaths::resolve_windows_cuda_payload_dir()` or the same `windows_cuda_payload_candidate_dirs()` list for `ggml_directory`, payload presence, and final CUDA runtime directory selection.
- [x] 16. Use `ProbeResult` in launcher backend selection: CUDA is selectable only when driver/device, runtime usability, payload presence, and backend loadability are all true.
- [x] 17. Change the CUDA prompt text: installed users need a repaired app/runtime package or compatible redistributable runtime; source builders need the full Toolkit.
- [x] 17a. Gate the CUDA repair/download prompt by failure reason so driver-present/no-device cases fall back instead of prompting for runtime repair.
- [x] 18. In `build_llama_windows.ps1`, after resolving `$cudaRoot`, copy `cudart64_*.dll`, `cublas64_*.dll`, and `cublasLt64_*.dll` from `$cudaRoot\bin\x64` then `$cudaRoot\bin`.
- [x] 18a. Treat CUDA toolkit roots without `bin\nvcc.exe` as unusable before CMake configuration.
- [x] 19. Copy those DLLs into both `app\lib\precompiled\cuda\bin` and `app\lib\ggml\wcuda`.
- [x] 20. Fail the CUDA build if the three DLL patterns are missing after staging.
- [x] 21. In `build_windows.ps1`, verify packaged CUDA payloads before distribution when `lib/precompiled/cuda/bin` exists.
- [x] 22. Add a reusable artifact audit that checks staged `lib\ggml\wcuda` payloads contain `llama.dll`, `ggml.dll`, `ggml-cuda.dll`, `cudart64_*`, `cublas64_*`, and `cublasLt64_*`.
- [x] 23. Document that generated DLLs remain ignored and must not be committed.
- [x] 24. Add benchmark probe-signature generation from driver version, device count, runtime DLL name/source, backend loadability, app version, and benchmark backend environment controls.
- [x] 25. Save the signature at benchmark finish.
- [x] 26. Treat `SuitabilityBenchmarkCompleted=true` as current only when saved signature matches the current probe signature.
- [x] 27. When stale, auto-show the benchmark unless suppressed and prepend a “previous result may be stale” line to old report rendering.
- [x] 28. Do not use logs or SQLite DB files as CUDA state; logs remain diagnostic evidence only.
- [x] 29. Add unit tests for packaged CUDA dir priority, Toolkit/PhysX ordering, mixed CUDA suffix ranking, no-system-dir probe option behavior, and source-aware Packaged > ToolkitHint > PATH ordering.
- [x] 30. Add Settings-level tests for benchmark signature match/mismatch, schema staleness, incomplete benchmark state, backend environment-control coverage, and document all new tests in `TESTS.md`.
- [x] 30a. Add/repair the CUDA payload resolver test so it fails until `cudart64_*`, `cublas64_*`, and `cublasLt64_*` are all present.
- [x] 30b. Add direct tests for `SuitabilityBenchmarkDialog::finish_benchmark()` saving the generated signature/schema.
- [x] 30c. Add UI/main-app tests proving stale completed benchmarks auto-show when unsuppressed and previous-result rendering prepends the stale warning.
- [x] 30d. Keep reduced-source Windows test targets building when shared helpers gain dependencies; `ai_file_sorter_updater_notify_only_tests` and `ai_file_sorter_updater_disabled_tests` must include `Utils.cpp` dependencies such as `GgmlRuntimePaths.cpp`.
- [x] 30e. Add or run a UI/startup verification proving suppressed benchmark prompts return before computing the current probe signature.

## Contribution Steps

- [x] 31. Run targeted tests: `ai_file_sorter_tests "WindowsCudaProbe*"`, `ai_file_sorter_tests "Windows CUDA payload*"`, and benchmark-signature Settings tests.
- [x] 32. Run Windows CPU CI parity: `app\scripts\build_llama_windows.ps1 cuda=off vulkan=off blas=off vcpkgroot=<path>`.
- [x] 33. Run full Windows test build: `app\build_windows.ps1 -Configuration Release -VcpkgRoot <path> -BuildTests -RunTests -Parallel 4`.
- [ ] 34. Run CUDA packaging proof: `app\scripts\build_llama_windows.ps1 cuda=on vulkan=off vcpkgroot=<path>`.
- [ ] 35. Inspect the staged artifact, not source dirs: `app\build-windows\Release\lib\ggml\wcuda`.
- [ ] 35a. Inspect every staged Windows variant when using the default build set: `app\build-windows\Release\lib\ggml\wcuda`, `app\build-windows-store\Release\lib\ggml\wcuda`, and `app\build-windows-standalone\Release\lib\ggml\wcuda`.
- [ ] 36. Smoke-test portable/installer behavior with NVIDIA driver present and no Toolkit on `PATH` if a clean VM is available.
- [ ] 36a. Run a dependency audit such as `dumpbin /dependents` on the built `ggml-cuda.dll`; confirm every imported NVIDIA CUDA DLL family is both redistributable under CUDA Toolkit EULA Attachment A and covered by staging/audit checks.
- [ ] 36b. Smoke-test an incomplete packaged CUDA payload with Toolkit/PATH CUDA dirs removed or hidden, including cases missing `cublas64_*` and `cublasLt64_*`, to prove the launcher does not accept a partial package.
- [x] 37. Prepare a sanitized PR note: repro matrix, root cause, changed detection model, staged DLL evidence, and tests run.
- [x] 38. Commit one focused change: `fix(runtime): harden Windows CUDA detection and staging`.
- [ ] 39. Push to the fork branch and open a PR against `hyperfield/ai-file-sorter:dev`.
- [ ] 40. After PR creation, open an issue only if maintainers prefer issue-first tracking; otherwise reference the PR as the fix and include sanitized AppData/log observations.

## Assumptions

- Target branch is `upstream/dev` because it contains the current Windows runtime work and is ahead of `main`.
- The first upstream-friendly fix is source/script/docs only; no generated DLLs, build folders, `app/lib/ggml`, or `app/lib/precompiled*` are committed.
- CUDA runtime DLLs must match the Toolkit used to build `ggml-cuda.dll`; do not chase the newest CUDA release during this fix.
