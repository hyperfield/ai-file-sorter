# 2025-11-04: fix(splash-screen): adjust splash screen appearance to prevent its unexpected persistence on Windows

## Covered commits
- `bcd9543` `2025-11-04` `fix(splash-screen): adjust splash screen appearance to prevent its unexpected persistence on Windows`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/Utils.cpp`, `app/main.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(splash-screen): adjust splash screen appearance to prevent its unexpected persistence on Windows`.

Before this commit, the repository reflected the state immediately preceding `bcd9543`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -326,8 +326,19 @@ bool Utils::is_valid_directory(const char *path)
     if (!path || *path == '\0') {
         return false;
     }
+#ifdef _WIN32
+    std::filesystem::path fs_path;
+    try {
+        fs_path = utf8_to_path(path);
+    } catch (const std::exception&) {
+        return false;
+    }
+#else
+    std::filesystem::path fs_path(path);
+#endif
+
     std::error_code ec;
-    return std::filesystem::is_directory(std::filesystem::path(path), ec);
+    return std::filesystem::is_directory(fs_path, ec);
 }
 
 namespace {
diff --git a/app/main.cpp b/app/main.cpp
index 338703b..52d6b40 100644
--- a/app/main.cpp
+++ b/app/main.cpp
@@ -85,7 +85,6 @@ int main(int argc, char **argv) {
         painter.end();
 
         QSplashScreen splash(splash_canvas);
-        splash.setWindowFlag(Qt::WindowStaysOnTopHint);
         const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
         splash.showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
         splash.show();
```

The excerpt is taken from the commit diff for `fix(splash-screen): adjust splash screen appearance to prevent its unexpected persistence on Windows`. The most relevant surfaces are `app/lib/Utils.cpp`, `app/main.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
