#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build"
mkdir -p "$BUILD_DIR"
SRC="$BUILD_DIR/translation_manager_test.cpp"
cat > "$SRC" <<'CPP'
#include "TranslationManager.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <iostream>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    TranslationManager::instance().initialize(qApp);

    TranslationManager::instance().set_language(Language::French);
    const QString french = QCoreApplication::translate("QObject", "Analyze folder");
    if (french != QStringLiteral("Analyser le dossier")) {
        std::cerr << "Expected French translation, got: " << french.toStdString() << "\n";
        return 1;
    }

    TranslationManager::instance().set_language(Language::English);
    const QString english = QCoreApplication::translate("QObject", "Analyze folder");
    if (english != QStringLiteral("Analyze folder")) {
        std::cerr << "Expected English fallback, got: " << english.toStdString() << "\n";
        return 2;
    }

    std::cout << "Translation manager test passed" << std::endl;
    return 0;
}
CPP

OUTPUT="$BUILD_DIR/translation_manager_test"

QT_INCLUDES=(
    -I"$ROOT_DIR/app/include"
    -I/usr/include/x86_64-linux-gnu/qt6
    -I/usr/include/x86_64-linux-gnu/qt6/QtCore
    -I/usr/include/x86_64-linux-gnu/qt6/QtGui
    -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets
)

QT_LIBS=(
    -lQt6Core
    -lQt6Gui
    -lQt6Widgets
)

g++ -std=c++20 -fPIC "${QT_INCLUDES[@]}" "$SRC" "$ROOT_DIR/app/lib/TranslationManager.cpp" -o "$OUTPUT" "${QT_LIBS[@]}"
QT_QPA_PLATFORM=offscreen "$OUTPUT"
