# 2025-11-11: chore(main): refactor for more readable code

## Covered commits
- `4b228e4` `2025-11-11` `chore(main): refactor for more readable code`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/main.cpp`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/main.cpp`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `4b228e4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/main.cpp b/app/main.cpp
--- a/app/main.cpp
+++ b/app/main.cpp
@@ -19,6 +19,7 @@
 #include <vector>
 #include <cstring>
 #include <QPainter>
+#include <memory>
 
 #include <curl/curl.h>
 #include <locale.h>
@@ -45,18 +46,199 @@ bool initialize_loggers()
     }
 }
 
+namespace {
 
-int main(int argc, char **argv) {
+struct ParsedArguments {
+    bool development_mode{false};
+    std::vector<char*> qt_args;
+};
+
+ParsedArguments parse_command_line(int argc, char** argv)
+{
+    ParsedArguments parsed;
+    parsed.qt_args.reserve(static_cast<size_t>(argc) + 1);
+
+    for (int i = 0; i < argc; ++i) {
+        const bool is_flag = (i > 0);
+        if (is_flag && std::strcmp(argv[i], "--development") == 0) {
+            parsed.development_mode = true;
+            continue;
+        }
+        if (is_flag && std::strcmp(argv[i], "--allow-direct-launch") == 0) {
+            continue;
+        }
+        parsed.qt_args.push_back(argv[i]);
+    }
+    parsed.qt_args.push_back(nullptr);
+    return parsed;
+}
 
 #ifdef _WIN32
-    bool allow_direct_launch = false;
+bool allow_direct_launch(int argc, char** argv)
+{
     for (int i = 1; i < argc; ++i) {
         if (std::strcmp(argv[i], "--allow-direct-launch") == 0) {
-            allow_direct_launch = true;
-            break;
+            return true;
+        }
+    }
+    return false;
+}
+#endif
+
+QPixmap build_splash_pixmap()
+{
+    QPixmap splash_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/icon_512x512.png"));
+    if (splash_pix.isNull()) {
+        splash_pix = QPixmap(256, 256);
+        splash_pix.fill(Qt::black);
+    }
+
+    const QSize base_size(320, 320);
+    const QSize padded_size(static_cast<int>(base_size.width() * 1.2),
+                            static_cast<int>(base_size.height() * 1.1));
+
+    QPixmap scaled_splash = splash_pix.scaled(base_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
+    QPixmap splash_canvas(padded_size);
+    splash_canvas.fill(QColor(QStringLiteral("#f5e6d3")));
+
+    QPainter painter(&splash_canvas);
+    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
+
+    const QPoint centered_icon((padded_size.width() - scaled_splash.width()) / 2,
+                               (padded_size.height() - scaled_splash.height()) / 2 - 10);
+    painter.drawPixmap(centered_icon, scaled_splash);
+    painter.end();
+
+    return splash_canvas;
+}
+
+class SplashController {
+public:
+    explicit SplashController(QApplication& app)
+        : app_(app),
+          splash_(std::make_unique<QSplashScreen>(build_splash_pixmap()))
+    {
+        splash_->setWindowFlag(Qt::WindowStaysOnTopHint);
+        splash_->setWindowFlag(Qt::SplashScreen);
+        const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
+        splash_->showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
+        splash_->show();
+        raise();
+        QObject::connect(&app_, &QCoreApplication::aboutToQuit, splash_.get(), [this]() {
+            finish();
+        });
```

The excerpt is taken from the commit diff for `chore(main): refactor for more readable code`. The most relevant surfaces are `app/main.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
