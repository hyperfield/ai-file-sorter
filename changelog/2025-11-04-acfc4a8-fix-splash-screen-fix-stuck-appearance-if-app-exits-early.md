# 2025-11-04: fix(splash-screen): fix stuck appearance if app exits early

## Covered commits
- `acfc4a8` `2025-11-04` `fix(splash-screen): fix stuck appearance if app exits early`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/main.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(splash-screen): fix stuck appearance if app exits early`.

Before this commit, the repository reflected the state immediately preceding `acfc4a8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/main.cpp b/app/main.cpp
--- a/app/main.cpp
+++ b/app/main.cpp
@@ -15,6 +15,8 @@
 #include <QElapsedTimer>
 #include <QTimer>
 
+#include <functional>
+
 #include <curl/curl.h>
 #include <locale.h>
 #include <libintl.h>
@@ -85,6 +87,7 @@ int main(int argc, char **argv) {
         painter.end();
 
         QSplashScreen splash(splash_canvas);
+        splash.setWindowFlag(Qt::WindowStaysOnTopHint);
         const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
         splash.showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
         splash.show();
```

The excerpt is taken from the commit diff for `fix(splash-screen): fix stuck appearance if app exits early`. The most relevant surfaces are `app/main.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
