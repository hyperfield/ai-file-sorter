# 2025-11-08: chore(codacy): update

## Covered commits
- `13e91d1` `2025-11-08` `chore(codacy): update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.codacy/codacy.yaml`
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `.codacy/codacy.yaml`, `app/include/external/llama.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(codacy): update`.

Before this commit, the repository reflected the state immediately preceding `13e91d1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.codacy/codacy.yaml b/.codacy/codacy.yaml
--- a/.codacy/codacy.yaml
+++ b/.codacy/codacy.yaml
@@ -1,8 +1,15 @@
 runtimes:
+    - dart@3.7.2
+    - go@1.22.3
     - java@17.0.10
+    - node@22.2.0
     - python@3.11.11
 tools:
+    - dartanalyzer@3.7.2
+    - eslint@8.57.0
     - lizard@1.17.31
-    - pmd@6.55.0
+    - pmd@7.11.0
+    - pylint@3.3.6
+    - revive@1.7.0
     - semgrep@1.78.0
     - trivy@0.66.0
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ee09828..7675c55 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
```

The excerpt is taken from the commit diff for `chore(codacy): update`. The most relevant surfaces are `.codacy/codacy.yaml`, `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
