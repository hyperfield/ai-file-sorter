# 2025-08-25: Fix: Corrected typo in Debian/Ubuntu dependencies

## Covered commits
- `a6aee23` `2025-08-25` `Fix: Corrected typo in Debian/Ubuntu dependencies`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit corrected behavior in `README.md`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `Fix: Corrected typo in Debian/Ubuntu dependencies`.

Before this commit, the repository reflected the state immediately preceding `a6aee23`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -249,7 +249,7 @@ cd ai-file-sorter
 ##### Debian / Ubuntu
 
 ```bash
-sudo apt update && sudo apt install -y build-essential cmake libgtk-3-dev libgtkmm-3.0-dev libgdk-pixbuf2.0-dev libglib2.0-dev libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libx11-dev libxi-dev libxfixes-dev libcairo2-dev libatk1.0-dev libepoxy-dev libharfbuzz-dev libfontconfig1-dev libfmt-dev libspdlog-dev libpng-dev libjpeg-dev libffi-dev libpcre3-dev libnghttp2-dev libidn2-0-dev librtmp-dev libssh-dev libpsl-dev libkrb5-dev libldap2-dev libbrotli-dev libxcb1-dev libxrandr-dev libxinerama-dev libxkbcommon-dev libwayland-dev libthai-dev libfreetype6-dev libgraphite2-dev libgnutls28-dev libp11-kit-dev iblzma-dev liblz4-dev libgcrypt20-dev libsystemd-dev ocl-icd-opencl-dev
+sudo apt update && sudo apt install -y build-essential cmake libgtk-3-dev libgtkmm-3.0-dev libgdk-pixbuf2.0-dev libglib2.0-dev libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libx11-dev libxi-dev libxfixes-dev libcairo2-dev libatk1.0-dev libepoxy-dev libharfbuzz-dev libfontconfig1-dev libfmt-dev libspdlog-dev libpng-dev libjpeg-dev libffi-dev libpcre3-dev libnghttp2-dev libidn2-0-dev librtmp-dev libssh-dev libpsl-dev libkrb5-dev libldap2-dev libbrotli-dev libxcb1-dev libxrandr-dev libxinerama-dev libxkbcommon-dev libwayland-dev libthai-dev libfreetype6-dev libgraphite2-dev libgnutls28-dev libp11-kit-dev liblzma-dev liblz4-dev libgcrypt20-dev libsystemd-dev ocl-icd-opencl-dev
 ```
 
 ##### Fedora / RedHat
```

The excerpt is taken from the commit diff for `Fix: Corrected typo in Debian/Ubuntu dependencies`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
