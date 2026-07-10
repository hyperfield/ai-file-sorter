# 2026-01-11: feat(categorization-dialog): add image file icon

## Covered commits
- `54a8cd1` `2026-01-11` `feat(categorization-dialog): add image file icon`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/CategorizationDialog.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/CategorizationDialog.cpp`. It changed the project from not having the capability described by `feat(categorization-dialog): add image file icon` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `54a8cd1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -30,6 +30,10 @@
 #include <QFile>
 #include <QFileIconProvider>
 #include <QFileInfo>
+#include <QPainter>
+#include <QPen>
+#include <QPixmap>
+#include <QPolygonF>
 #include <QDir>
 #include <QJsonArray>
 #include <QJsonDocument>
@@ -341,6 +345,58 @@ QFileIconProvider& file_icon_provider()
     return provider;
 }
 
+bool icons_match(const QIcon& lhs, const QIcon& rhs, const QSize& size)
+{
+    const QPixmap lhs_pixmap = lhs.pixmap(size);
+    const QPixmap rhs_pixmap = rhs.pixmap(size);
+    if (lhs_pixmap.isNull() || rhs_pixmap.isNull()) {
+        return false;
+    }
+    return lhs_pixmap.toImage() == rhs_pixmap.toImage();
+}
+
+QIcon fallback_image_icon()
+{
+    static QIcon icon;
+    if (!icon.isNull()) {
+        return icon;
+    }
+
+    auto make_pixmap = [](int size) {
+        QPixmap pixmap(size, size);
+        pixmap.fill(Qt::transparent);
+
+        QPainter painter(&pixmap);
+        painter.setRenderHint(QPainter::Antialiasing, true);
+        QPen frame_pen(QColor(120, 120, 120));
+        frame_pen.setWidthF(1.0);
+        painter.setPen(frame_pen);
+        painter.setBrush(QColor(240, 240, 240));
+        painter.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 2, 2);
+
+        QRectF image_rect(3, 4, size - 6, size - 7);
+        painter.setPen(Qt::NoPen);
+        painter.setBrush(QColor(140, 200, 245));
+        painter.drawRect(image_rect);
+
+        QPolygonF mountain;
+        mountain << QPointF(image_rect.left() + 1, image_rect.bottom() - 1)
+                 << QPointF(image_rect.center().x() - 1, image_rect.top() + 2)
+                 << QPointF(image_rect.right() - 1, image_rect.bottom() - 1);
+        painter.setBrush(QColor(90, 170, 125));
+        painter.drawPolygon(mountain);
+
+        painter.setBrush(QColor(255, 210, 80));
+        painter.drawEllipse(QPointF(image_rect.right() - 3, image_rect.top() + 3), 1.6, 1.6);
+
+        return pixmap;
+    };
+
+    icon.addPixmap(make_pixmap(16));
+    icon.addPixmap(make_pixmap(32));
+    return icon;
+}
+
 QIcon type_icon(const QString& code, const QString& file_path)
 {
     if (auto* style = QApplication::style()) {
```

The excerpt is taken from the commit diff for `feat(categorization-dialog): add image file icon`. The most relevant surfaces are `app/lib/CategorizationDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
