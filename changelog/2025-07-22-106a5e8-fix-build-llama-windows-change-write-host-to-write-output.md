# 2025-07-22: fix(build-llama-windows): change Write-Host to Write-Output

## Covered commits
- `106a5e8` `2025-07-22` `fix(build-llama-windows): change Write-Host to Write-Output`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit corrected behavior in `app/scripts/build_llama_windows.ps1`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(build-llama-windows): change Write-Host to Write-Output`.

Before this commit, the repository reflected the state immediately preceding `106a5e8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_windows.ps1 b/app/scripts/build_llama_windows.ps1
--- a/app/scripts/build_llama_windows.ps1
+++ b/app/scripts/build_llama_windows.ps1
@@ -73,4 +73,4 @@ Copy-Item "$llamaDir\include\llama.h" -Destination $headersDir
 Copy-Item "$llamaDir\ggml\src\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
 Copy-Item "$llamaDir\ggml\include\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
 
-Write-Host "`n Build complete. Precompiled DLLs updated in: $precompiledLibsDir`n"
\ No newline at end of file
+Write-Output "`n Build complete. Precompiled DLLs updated in: $precompiledLibsDir`n"
\ No newline at end of file
```

The excerpt is taken from the commit diff for `fix(build-llama-windows): change Write-Host to Write-Output`. The most relevant surfaces are `app/scripts/build_llama_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
