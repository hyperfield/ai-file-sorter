# 2025-11-22: chore(github): add workflow

## Covered commits
- `7c665ec` `2025-11-22` `chore(github): add workflow`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `.github/workflows/build.yml`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `.github/workflows/build.yml`. It changed the repository support state, metadata, or supporting files in the way described by `chore(github): add workflow`.

Before this commit, the repository reflected the state immediately preceding `7c665ec`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.github/workflows/build.yml b/.github/workflows/build.yml
--- /dev/null
+++ b/.github/workflows/build.yml
@@ -0,0 +1,39 @@
+name: Build
+
+on:
+  push:
+    branches: [ main, dev ]
+  pull_request:
+    branches: [ main, dev ]
+
+jobs:
+  build:
+    runs-on: ${{ matrix.os }}
+    strategy:
+      matrix:
+        os: [ubuntu-latest]
+    steps:
+      - name: Check out repository
+        uses: actions/checkout@v4
+
+      - name: Set up Qt
+        uses: jurplel/install-qt-action@v3
+        with:
+          version: '6.6.3'
+          host: linux
+          target: desktop
+
+      - name: Install dependencies
+        run: |
+          sudo apt-get update
+          sudo apt-get install -y build-essential cmake ninja-build libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
+
+      - name: Configure
+        run: |
+          cmake -S app -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
+
+      - name: Build
+        run: cmake --build build
+
+      - name: Run ctest
+        run: cd build && ctest --output-on-failure
```

The excerpt is taken from the commit diff for `chore(github): add workflow`. The most relevant surfaces are `.github/workflows/build.yml`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
