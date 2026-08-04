# 2025-06-18: chore(submodule): properly add llama.cpp as a submodule

## Covered commits
- `af5890c` `2025-06-18` `chore(submodule): properly add llama.cpp as a submodule`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.gitignore`
- `A` `.gitmodules`
- `M` `README.md`
- `A` `app/include/external/llama.cpp`
- `A` `exclude-paths.txt`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `.gitignore`, `.gitmodules`, `README.md`, `app/include/external/llama.cpp`, `exclude-paths.txt`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `af5890c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -40,9 +40,6 @@ encryption.ini
 *.a
 *.lib
 
-# Third-part libraries
-app/include/external/llama.cpp/
-
 # Executables
 *.exe
 *.out
diff --git a/.gitmodules b/.gitmodules
new file mode 100644
index 0000000..b32414d
--- /dev/null
+++ b/.gitmodules
@@ -0,0 +1,3 @@
+[submodule "app/include/external/llama.cpp"]
+	path = app/include/external/llama.cpp
+	url = https://github.com/ggerganov/llama.cpp.git
```

The excerpt is taken from the commit diff for `chore(submodule): properly add llama.cpp as a submodule`. The most relevant surfaces are `.gitignore`, `.gitmodules`, `README.md`, `app/include/external/llama.cpp`, `exclude-paths.txt`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
