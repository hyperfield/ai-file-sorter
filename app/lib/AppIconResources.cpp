#include "AppIconResources.hpp"

#include <QFile>
#include <QSize>

#include <array>

namespace {

struct IconResourceEntry {
    const char* path;
    QSize size;
};

constexpr std::array<IconResourceEntry, 6> kWindowIconResources{{
    {":/dev/hfstudio/AIFileSorter/images/icon_16x16.png", QSize(16, 16)},
    {":/dev/hfstudio/AIFileSorter/images/icon_32x32.png", QSize(32, 32)},
    {":/dev/hfstudio/AIFileSorter/images/icon_64x64.png", QSize(64, 64)},
    {":/dev/hfstudio/AIFileSorter/images/icon_128x128.png", QSize(128, 128)},
    {":/dev/hfstudio/AIFileSorter/images/icon_256x256.png", QSize(256, 256)},
    {":/dev/hfstudio/AIFileSorter/images/icon_512x512.png", QSize(512, 512)},
}};

constexpr auto kFallbackLogoResource = ":/dev/hfstudio/AIFileSorter/images/logo.png";

} // namespace

namespace AppIconResources {

QIcon build_window_icon()
{
    QIcon icon;
    bool added_any = false;
    for (const auto& entry : kWindowIconResources) {
        if (!QFile::exists(QString::fromUtf8(entry.path))) {
            continue;
        }
        icon.addFile(QString::fromUtf8(entry.path), entry.size);
        added_any = true;
    }

    if (!added_any || icon.isNull()) {
        return QIcon(QString::fromUtf8(kFallbackLogoResource));
    }
    return icon;
}

} // namespace AppIconResources
