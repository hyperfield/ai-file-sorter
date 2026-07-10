# 2025-11-23: fix(workflows): update

## Covered commits
- `66d3ae1` `2025-11-23` `fix(workflows): update`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.github/workflows/build.yml`
- `M` `.gitignore`
- `M` `app/include/external/llama.cpp`

## What changed from what, why, and how
The commit corrected behavior in `.github/workflows/build.yml`, `.gitignore`, `app/include/external/llama.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(workflows): update`.

Before this commit, the repository reflected the state immediately preceding `66d3ae1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.github/workflows/build.yml b/.github/workflows/build.yml
--- a/.github/workflows/build.yml
+++ b/.github/workflows/build.yml
@@ -15,6 +15,8 @@ jobs:
     steps:
       - name: Check out repository
         uses: actions/checkout@v4
+        with:
+          submodules: recursive
 
       - name: Set up Qt
         uses: jurplel/install-qt-action@v3
diff --git a/.gitignore b/.gitignore
index e4f1221..2a764ba 100644
--- a/.gitignore
+++ b/.gitignore
@@ -109,6 +109,7 @@ app/include/llama/*.h
 /logo-workdir-temp/
 /Testing/
 /changelog/
+/models/
 
 # Build artifacts
```

The excerpt is taken from the commit diff for `fix(workflows): update`. The most relevant surfaces are `.github/workflows/build.yml`, `.gitignore`, `app/include/external/llama.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
