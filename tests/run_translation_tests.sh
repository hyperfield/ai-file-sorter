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

pkg_includes="$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"
pkg_libs="$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"

if [[ -n "$pkg_includes" && -n "$pkg_libs" ]]; then
    QT_FLAGS="$pkg_includes -I$ROOT_DIR/app/include"
    QT_LIB_FLAGS="$pkg_libs"
else
    qt_headers=""
    qt_libs=""
    if command -v qmake6 >/dev/null 2>&1; then
        qt_headers="$(qmake6 -query QT_INSTALL_HEADERS 2>/dev/null || true)"
        qt_libs="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
    elif command -v qmake >/dev/null 2>&1; then
        qt_headers="$(qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)"
        qt_libs="$(qmake -query QT_INSTALL_LIBS 2>/dev/null || true)"
    elif command -v qtpaths6 >/dev/null 2>&1; then
        prefix="$(qtpaths6 --install-prefix 2>/dev/null || true)"
        if [[ -n "$prefix" ]]; then
            qt_headers="$prefix/include"
            qt_libs="$prefix/lib"
        fi
    elif command -v brew >/dev/null 2>&1; then
        prefix="$(brew --prefix qt 2>/dev/null || brew --prefix qt6 2>/dev/null || true)"
        if [[ -n "$prefix" ]]; then
            qt_headers="$prefix/include"
            qt_libs="$prefix/lib"
        fi
    fi

    if [[ -n "$qt_headers" ]]; then
        QT_FLAGS="-I$ROOT_DIR/app/include -I$qt_headers -I$qt_headers/QtCore -I$qt_headers/QtGui -I$qt_headers/QtWidgets"
        if [[ -n "$qt_libs" ]]; then
            for fw in QtCore QtGui QtWidgets; do
                fw_headers="$qt_libs/$fw.framework/Headers"
                if [[ -d "$fw_headers" ]]; then
                    QT_FLAGS="$QT_FLAGS -I$fw_headers"
                fi
            done
        fi
    else
        QT_FLAGS="-I$ROOT_DIR/app/include -I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets -I/opt/homebrew/include -I/opt/homebrew/include/QtCore -I/opt/homebrew/include/QtGui -I/opt/homebrew/include/QtWidgets"
    fi

    if [[ -n "$qt_libs" ]]; then
        if [[ -d "$qt_libs/QtCore.framework" ]]; then
            QT_LIB_FLAGS="-F$qt_libs -framework QtCore -framework QtGui -framework QtWidgets"
        else
            QT_LIB_FLAGS="-L$qt_libs -lQt6Core -lQt6Gui -lQt6Widgets"
        fi
    else
        QT_LIB_FLAGS="-L/opt/homebrew/lib -lQt6Core -lQt6Gui -lQt6Widgets"
    fi
fi

# shellcheck disable=SC2086
g++ -std=c++20 -fPIC $QT_FLAGS "$SRC" "$ROOT_DIR/app/lib/TranslationManager.cpp" -o "$OUTPUT" $QT_LIB_FLAGS
QT_QPA_PLATFORM=offscreen "$OUTPUT"
