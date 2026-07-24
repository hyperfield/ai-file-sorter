#include "WindowsNetworkLocations.hpp"

#include "Settings.hpp"

#include <algorithm>
#include <optional>

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <windows.h>
#endif

namespace {

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString to_qstring(const std::string& value)
{
    return QString::fromStdString(value);
}

QString normalized_native_path(QString path)
{
    return QDir::toNativeSeparators(path.trimmed());
}

#if defined(Q_OS_WIN)
struct NetworkLocationCandidate {
    QString label;
    QString path;
};

bool is_unc_qt_path(const QString& path)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    return normalized.startsWith(QStringLiteral("//"));
}

QString unc_share_root_qt(const QString& path)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    if (!normalized.startsWith(QStringLiteral("//"))) {
        return {};
    }

    const QStringList parts = normalized.mid(2).split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return {};
    }

    return normalized_native_path(QStringLiteral("//%1/%2").arg(parts.at(0), parts.at(1)));
}

bool is_drive_root_qt(const QString& path)
{
    const QString native_path = normalized_native_path(path);
    return native_path.size() >= 3 &&
           native_path.at(1) == QLatin1Char(':') &&
           (native_path.at(2) == QLatin1Char('\\') || native_path.at(2) == QLatin1Char('/'));
}

bool is_remote_drive_root_qt(const QString& path)
{
    if (!is_drive_root_qt(path)) {
        return false;
    }

    const QString native_path = normalized_native_path(path);
    const UINT drive_type = GetDriveTypeW(reinterpret_cast<LPCWSTR>(native_path.utf16()));
    return drive_type == DRIVE_REMOTE;
}

bool is_network_location_qt_path(const QString& path)
{
    return is_unc_qt_path(path) || is_remote_drive_root_qt(path);
}

void append_unique_network_location(std::vector<NetworkLocationCandidate>& entries,
                                    QString label,
                                    QString path)
{
    path = normalized_native_path(std::move(path));
    label = label.trimmed();
    if (path.isEmpty() || !is_network_location_qt_path(path)) {
        return;
    }
    if (label.isEmpty()) {
        label = path;
    }

    const auto duplicate = std::find_if(entries.begin(), entries.end(), [&](const NetworkLocationCandidate& entry) {
        return entry.path.compare(path, Qt::CaseInsensitive) == 0;
    });
    if (duplicate == entries.end()) {
        entries.push_back(NetworkLocationCandidate{std::move(label), std::move(path)});
    }
}

std::optional<QString> known_net_hood_path()
{
    wchar_t path[MAX_PATH] = {};
    const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_NETHOOD, nullptr, SHGFP_TYPE_CURRENT, path);
    if (FAILED(hr) || path[0] == L'\0') {
        return std::nullopt;
    }

    QString result = QString::fromWCharArray(path);
    if (result.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<QString> resolve_shortcut_target(const QString& shortcut_path)
{
    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool should_uninitialize = SUCCEEDED(init_hr);
    if (FAILED(init_hr) && init_hr != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    IShellLinkW* shell_link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  __uuidof(IShellLinkW),
                                  reinterpret_cast<void**>(&shell_link));
    if (FAILED(hr) || !shell_link) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    IPersistFile* persist_file = nullptr;
    hr = shell_link->QueryInterface(__uuidof(IPersistFile), reinterpret_cast<void**>(&persist_file));
    if (SUCCEEDED(hr) && persist_file) {
        hr = persist_file->Load(reinterpret_cast<LPCOLESTR>(shortcut_path.utf16()), STGM_READ);
    }

    wchar_t target[MAX_PATH] = {};
    if (SUCCEEDED(hr)) {
        hr = shell_link->GetPath(target, MAX_PATH, nullptr, SLGP_UNCPRIORITY);
    }

    if (persist_file) {
        persist_file->Release();
    }
    shell_link->Release();
    if (should_uninitialize) {
        CoUninitialize();
    }

    if (FAILED(hr) || target[0] == L'\0') {
        return std::nullopt;
    }
    return QString::fromWCharArray(target);
}

QString network_shortcut_label(const QFileInfo& shortcut)
{
    if (shortcut.completeBaseName().compare(QStringLiteral("target"), Qt::CaseInsensitive) == 0) {
        const QString parent_name = shortcut.dir().dirName().trimmed();
        if (!parent_name.isEmpty()) {
            return parent_name;
        }
    }
    return shortcut.completeBaseName();
}

std::vector<WindowsNetworkLocation> to_public_entries(const std::vector<NetworkLocationCandidate>& candidates)
{
    std::vector<WindowsNetworkLocation> entries;
    entries.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        entries.push_back(WindowsNetworkLocation{
            .label = to_utf8(candidate.label),
            .path = to_utf8(candidate.path)
        });
    }
    return entries;
}
#endif

} // namespace

std::vector<WindowsNetworkLocation> WindowsNetworkLocations::discover(const Settings& settings)
{
#if defined(Q_OS_WIN)
    std::vector<NetworkLocationCandidate> entries;

    for (const QFileInfo& drive : QDir::drives()) {
        const QString drive_path = drive.absoluteFilePath();
        if (is_remote_drive_root_qt(drive_path)) {
            append_unique_network_location(entries, normalized_native_path(drive_path), drive_path);
        }
    }

    if (const auto net_hood = known_net_hood_path()) {
        QDirIterator shortcuts(*net_hood,
                               QStringList{QStringLiteral("*.lnk")},
                               QDir::Files | QDir::NoSymLinks,
                               QDirIterator::Subdirectories);
        while (shortcuts.hasNext()) {
            shortcuts.next();
            const QFileInfo shortcut_info(shortcuts.filePath());
            const auto target = resolve_shortcut_target(shortcut_info.absoluteFilePath());
            if (target) {
                append_unique_network_location(entries, network_shortcut_label(shortcut_info), *target);
            }
        }
    }

    for (const auto& location : settings.get_recent_network_locations()) {
        const QString path = to_qstring(location);
        append_unique_network_location(entries, path, path);
    }

    const QString sort_folder_root = unc_share_root_qt(to_qstring(settings.get_sort_folder()));
    if (!sort_folder_root.isEmpty()) {
        append_unique_network_location(entries, sort_folder_root, sort_folder_root);
    }

    return to_public_entries(entries);
#else
    Q_UNUSED(settings);
    return {};
#endif
}

bool WindowsNetworkLocations::is_unc_path(const std::string& path)
{
#if defined(Q_OS_WIN)
    return is_unc_qt_path(to_qstring(path));
#else
    Q_UNUSED(path);
    return false;
#endif
}

bool WindowsNetworkLocations::is_remote_drive_root(const std::string& path)
{
#if defined(Q_OS_WIN)
    return is_remote_drive_root_qt(to_qstring(path));
#else
    Q_UNUSED(path);
    return false;
#endif
}

bool WindowsNetworkLocations::is_network_location_path(const std::string& path)
{
#if defined(Q_OS_WIN)
    return is_network_location_qt_path(to_qstring(path));
#else
    Q_UNUSED(path);
    return false;
#endif
}

std::string WindowsNetworkLocations::unc_share_root(const std::string& path)
{
#if defined(Q_OS_WIN)
    return to_utf8(unc_share_root_qt(to_qstring(path)));
#else
    Q_UNUSED(path);
    return {};
#endif
}
