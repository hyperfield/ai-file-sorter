# 2026-03-14: fix(packaging): add missing Debian runtime dependencies

## Covered commits
- `96c567f` `2026-03-14` `fix(packaging): add missing Debian runtime dependencies`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/package_deb.sh`

## What changed from what, why, and how
The commit corrected behavior in `app/scripts/package_deb.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(packaging): add missing Debian runtime dependencies`.

Before this commit, the repository reflected the state immediately preceding `96c567f`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/package_deb.sh b/app/scripts/package_deb.sh
--- a/app/scripts/package_deb.sh
+++ b/app/scripts/package_deb.sh
@@ -121,7 +121,7 @@ get_needed_soname() {
 
 resolve_fmt_dep() {
     local soname
-    soname="$(get_needed_soname "$BIN_PATH" '^libfmt\.so')"
+    soname="$(get_needed_soname "$BIN_PATH" '^libfmt[.]so')"
     case "$soname" in
         libfmt.so.10) echo "libfmt10" ;;
         libfmt.so.9) echo "libfmt9" ;;
@@ -132,7 +132,7 @@ resolve_fmt_dep() {
 
 resolve_jsoncpp_dep() {
     local soname
-    soname="$(get_needed_soname "$BIN_PATH" '^libjsoncpp\.so')"
+    soname="$(get_needed_soname "$BIN_PATH" '^libjsoncpp[.]so')"
     case "$soname" in
         libjsoncpp.so.26) echo "libjsoncpp26" ;;
         libjsoncpp.so.25) echo "libjsoncpp25" ;;
```

The excerpt is taken from the commit diff for `fix(packaging): add missing Debian runtime dependencies`. The most relevant surfaces are `app/scripts/package_deb.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
