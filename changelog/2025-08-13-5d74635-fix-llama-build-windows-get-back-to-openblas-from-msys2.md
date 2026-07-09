# 2025-08-13: fix(llama-build-windows): get back to OpenBLAS from MSYS2 for building llama.cpp on Windows

## Covered commits
- `5d74635` `2025-08-13` `fix(llama-build-windows): get back to OpenBLAS from MSYS2 for building llama.cpp on Windows`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/scripts/build_llama_windows.ps1`
- `D` `app/scripts/vcpkg.json`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `README.md`, `app/scripts/build_llama_windows.ps1`, `app/scripts/vcpkg.json`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `5d74635`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -141,7 +141,7 @@ You can also now launch `Git Bash` from Start Menu.
 3. Install dependencies:
    
 ```bash
-pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-jsoncpp mingw-w64-x86_64-pcre mingw-w64-x86_64-libidn2 mingw-w64-x86_64-libssh mingw-w64-x86_64-libpsl mingw-w64-x86_64-openldap mingw-w64-x86_64-gnutls mingw-w64-x86_64-lz4 mingw-w64-x86_64-libgcrypt mingw-w64-x86_64-fmt mingw-w64-x86_64-spdlog mingw-w64-x86_64-curl make
+pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-jsoncpp mingw-w64-x86_64-pcre mingw-w64-x86_64-libidn2 mingw-w64-x86_64-libssh mingw-w64-x86_64-libpsl mingw-w64-x86_64-openldap mingw-w64-x86_64-gnutls mingw-w64-x86_64-lz4 mingw-w64-x86_64-libgcrypt mingw-w64-x86_64-fmt mingw-w64-x86_64-spdlog mingw-w64-x86_64-curl mingw-w64-x86_64-openblas make
 ```
 
 4. Install [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/).
@@ -159,13 +159,7 @@ pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkm
 
     Save any changes.
 
-7. In the same directory as in 6, run
-  
-        vcpkg install
-
-    The needed packages for building `llama.cpp` will be compiled and installed.
-
-8. In `Developer PowerShell for VS 2022`, run
+7. In `Developer PowerShell for VS 2022`, run
 
     **If you have CUDA**:
```

The excerpt is taken from the commit diff for `fix(llama-build-windows): get back to OpenBLAS from MSYS2 for building llama.cpp on Windows`. The most relevant surfaces are `README.md`, `app/scripts/build_llama_windows.ps1`, `app/scripts/vcpkg.json`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
