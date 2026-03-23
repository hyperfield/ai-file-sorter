#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build"
mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/i18n"

if rg -n '<translation type="unfinished"' "$ROOT_DIR"/app/resources/i18n/*.ts >/dev/null; then
    echo "Unfinished translations remain in app/resources/i18n" >&2
    rg -n '<translation type="unfinished"' "$ROOT_DIR"/app/resources/i18n/*.ts >&2
    exit 1
fi

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
    const QString french = QCoreApplication::translate("UiTranslator", "Analyze folder");
    if (french != QStringLiteral("Analyser le dossier")) {
        std::cerr << "Expected French translation, got: " << french.toStdString() << "\n";
        return 1;
    }

    TranslationManager::instance().set_language(Language::English);
    const QString english = QCoreApplication::translate("UiTranslator", "Analyze folder");
    if (english != QStringLiteral("Analyze folder")) {
        std::cerr << "Expected English fallback, got: " << english.toStdString() << "\n";
        return 2;
    }

    std::cout << "Translation manager test passed" << std::endl;
    return 0;
}
CPP

OUTPUT="$BUILD_DIR/translation_manager_test"

find_qt6_path_tool() {
    local tool_name="$1"
    local fallback_path="$2"
    if command -v "$tool_name" >/dev/null 2>&1; then
        command -v "$tool_name"
        return 0
    fi
    if [[ -x "$fallback_path" ]]; then
        printf '%s\n' "$fallback_path"
        return 0
    fi
    return 1
}

find_qt6_lrelease() {
    local candidate qt_host_dir

    for candidate in lrelease6 lrelease-qt6; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    if [[ -n "${QTPATHS6:-}" ]]; then
        while IFS= read -r qt_host_dir; do
            if [[ -n "$qt_host_dir" && -x "$qt_host_dir/lrelease" ]]; then
                printf '%s\n' "$qt_host_dir/lrelease"
                return 0
            fi
        done < <(
            "$QTPATHS6" --query QT_HOST_LIBEXECS 2>/dev/null || true
            "$QTPATHS6" --query QT_HOST_BINS 2>/dev/null || true
        )
    fi

    if [[ -n "${QMAKE6:-}" ]]; then
        while IFS= read -r qt_host_dir; do
            if [[ -n "$qt_host_dir" && -x "$qt_host_dir/lrelease" ]]; then
                printf '%s\n' "$qt_host_dir/lrelease"
                return 0
            fi
        done < <(
            "$QMAKE6" -query QT_HOST_LIBEXECS 2>/dev/null || true
            "$QMAKE6" -query QT_HOST_BINS 2>/dev/null || true
            "$QMAKE6" -query QT_INSTALL_BINS 2>/dev/null || true
        )
    fi

    for candidate in \
        /usr/lib/qt6/libexec/lrelease \
        /usr/lib/qt6/bin/lrelease \
        /opt/homebrew/bin/lrelease \
        /opt/homebrew/opt/qt/share/qt/libexec/lrelease \
        /opt/homebrew/opt/qtbase/share/qt/libexec/lrelease \
        /usr/local/opt/qtbase/share/qt/libexec/lrelease; do
        if [[ -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    if command -v lrelease >/dev/null 2>&1; then
        candidate="$(command -v lrelease)"
        if "$candidate" -version 2>&1 | grep -Eq 'Qt[^0-9]*6|version 6'; then
            printf '%s\n' "$candidate"
            return 0
        fi
    fi

    return 1
}

QMAKE6="$(find_qt6_path_tool qmake6 /usr/lib/qt6/bin/qmake6 || true)"
QTPATHS6="$(find_qt6_path_tool qtpaths6 /usr/lib/qt6/bin/qtpaths6 || true)"

pkg_includes="$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"
pkg_libs="$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets 2>/dev/null || true)"

if [[ -n "$pkg_includes" && -n "$pkg_libs" ]]; then
    QT_FLAGS="$pkg_includes -I$ROOT_DIR/app/include"
    QT_LIB_FLAGS="$pkg_libs"
else
    qt_headers=""
    qt_libs=""
    if [[ -n "$QMAKE6" ]]; then
        qt_headers="$("$QMAKE6" -query QT_INSTALL_HEADERS 2>/dev/null || true)"
        qt_libs="$("$QMAKE6" -query QT_INSTALL_LIBS 2>/dev/null || true)"
    elif command -v qmake >/dev/null 2>&1; then
        qt_headers="$(qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)"
        qt_libs="$(qmake -query QT_INSTALL_LIBS 2>/dev/null || true)"
    elif [[ -n "$QTPATHS6" ]]; then
        prefix="$("$QTPATHS6" --install-prefix 2>/dev/null || true)"
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

LRELEASE="$(find_qt6_lrelease || true)"

if [[ -z "$LRELEASE" ]]; then
    echo "Could not find a Qt 6 lrelease binary. Install qt6-l10n-tools or set LRELEASE=/path/to/qt6/lrelease" >&2
    exit 1
fi

"$LRELEASE" "$ROOT_DIR/app/resources/i18n/aifilesorter_fr.ts" -qm "$BUILD_DIR/i18n/aifilesorter_fr.qm"

# shellcheck disable=SC2086
g++ -std=c++20 -fPIC $QT_FLAGS "$SRC" "$ROOT_DIR/app/lib/TranslationManager.cpp" -o "$OUTPUT" $QT_LIB_FLAGS
QT_QPA_PLATFORM=offscreen "$OUTPUT"
