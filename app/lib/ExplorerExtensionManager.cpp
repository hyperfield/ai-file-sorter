#include "ExplorerExtensionManager.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

constexpr std::wstring_view kDownloadUrl = L"https://filesorter.app/windows-explorer-extension/";

#ifdef _WIN32

constexpr std::wstring_view kSettingsKey = L"Software\\HFStudio\\AIFileSorter\\ExplorerExtension";
constexpr std::wstring_view kProgressPathValue = L"ProgressPath";
constexpr std::wstring_view kExtensionPathValue = L"ExtensionPath";
constexpr std::wstring_view kPackageRepositoryKey =
    L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
constexpr std::wstring_view kPackageIdentityPrefix = L"Hyperfield.AIFileSorterforFileExplorer_";

bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix)
{
    return value.size() >= prefix.size() &&
           std::equal(prefix.cbegin(), prefix.cend(), value.cbegin(), [](wchar_t a, wchar_t b) {
               return towlower(a) == towlower(b);
           });
}

std::optional<std::wstring> read_registry_string(std::wstring_view value_name)
{
    HKEY key = nullptr;
    const std::wstring subkey(kSettingsKey);
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD type = REG_NONE;
    DWORD bytes = 0;
    const std::wstring value_name_copy(value_name);
    status = RegQueryValueExW(key, value_name_copy.c_str(), nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        RegCloseKey(key);
        return std::nullopt;
    }

    std::wstring value((bytes / sizeof(wchar_t)) + 1, L'\0');
    status = RegQueryValueExW(key,
                              value_name_copy.c_str(),
                              nullptr,
                              &type,
                              reinterpret_cast<BYTE*>(value.data()),
                              &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }
    value.resize(wcslen(value.c_str()));
    if (type == REG_EXPAND_SZ) {
        std::wstring expanded(32768, L'\0');
        const DWORD expanded_size =
            ExpandEnvironmentStringsW(value.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
        if (expanded_size > 0 && expanded_size < expanded.size()) {
            expanded.resize(expanded_size - 1);
            return expanded;
        }
    }
    return value;
}

std::optional<std::filesystem::path> existing_regular_file(std::wstring_view value)
{
    if (value.empty()) {
        return std::nullopt;
    }
    std::filesystem::path path{std::wstring(value)};
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec) && !ec) {
        return path;
    }
    return std::nullopt;
}

bool package_is_registered()
{
    HKEY key = nullptr;
    const std::wstring subkey(kPackageRepositoryKey);
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    std::array<wchar_t, 512> name{};
    for (DWORD index = 0;; ++index) {
        DWORD length = static_cast<DWORD>(name.size());
        status = RegEnumKeyExW(key, index, name.data(), &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (status == ERROR_SUCCESS &&
            starts_with_case_insensitive(std::wstring_view(name.data(), length), kPackageIdentityPrefix)) {
            RegCloseKey(key);
            return true;
        }
    }

    RegCloseKey(key);
    return false;
}

bool shell_execute(std::wstring_view target,
                   std::wstring_view parameters,
                   std::wstring_view working_directory,
                   std::string* error)
{
    const std::wstring target_copy(target);
    const std::wstring parameters_copy(parameters);
    const std::wstring working_directory_copy(working_directory);
    const HINSTANCE result = ShellExecuteW(nullptr,
                                           L"open",
                                           target_copy.c_str(),
                                           parameters.empty() ? nullptr : parameters_copy.c_str(),
                                           working_directory.empty() ? nullptr : working_directory_copy.c_str(),
                                           SW_SHOWNORMAL);
    const auto code = reinterpret_cast<INT_PTR>(result);
    if (code > 32) {
        return true;
    }
    if (error) {
        *error = "Windows could not open the requested item.";
        if (code > 0) {
            *error += " ShellExecute error " + std::to_string(code) + ".";
        }
    }
    return false;
}

bool launch_progress_executable(const std::filesystem::path& executable,
                                std::wstring_view parameters,
                                std::string* error)
{
    const std::wstring working_directory = executable.parent_path().wstring();
    return shell_execute(executable.wstring(), parameters, working_directory, error);
}

#endif

} // namespace

ExplorerExtensionManager::Status ExplorerExtensionManager::inspect() const
{
#ifdef _WIN32
    Status status;
    if (const auto progress = read_registry_string(kProgressPathValue)) {
        status.progress_executable = existing_regular_file(*progress);
    }
    if (const auto extension = read_registry_string(kExtensionPathValue)) {
        status.extension_dll = existing_regular_file(*extension);
    }
    status.package_registered = package_is_registered();

    if (status.progress_executable) {
        status.state = State::Installed;
    } else if (status.extension_dll || status.package_registered) {
        status.state = State::InstalledNeedsRepair;
    } else {
        status.state = State::NotInstalled;
    }
    return status;
#else
    return Status{};
#endif
}

ExplorerExtensionManager::State ExplorerExtensionManager::state() const
{
    return inspect().state;
}

bool ExplorerExtensionManager::can_open_settings() const
{
    return inspect().progress_executable.has_value();
}

bool ExplorerExtensionManager::can_open_activity_window() const
{
    return inspect().progress_executable.has_value();
}

bool ExplorerExtensionManager::open_install_page(std::string* error) const
{
#ifdef _WIN32
    return shell_execute(kDownloadUrl, std::wstring_view{}, std::wstring_view{}, error);
#else
    if (error) {
        *error = "The Windows Explorer extension is only available on Windows.";
    }
    return false;
#endif
}

bool ExplorerExtensionManager::open_settings(std::string* error) const
{
#ifdef _WIN32
    const auto status = inspect();
    if (!status.progress_executable) {
        if (error) {
            *error = "The Windows Explorer extension settings executable could not be found.";
        }
        return false;
    }
    return launch_progress_executable(*status.progress_executable, L"--settings", error);
#else
    if (error) {
        *error = "The Windows Explorer extension is only available on Windows.";
    }
    return false;
#endif
}

bool ExplorerExtensionManager::open_activity_window(std::string* error) const
{
#ifdef _WIN32
    const auto status = inspect();
    if (!status.progress_executable) {
        if (error) {
            *error = "The Windows Explorer extension activity executable could not be found.";
        }
        return false;
    }
    return launch_progress_executable(*status.progress_executable, {}, error);
#else
    if (error) {
        *error = "The Windows Explorer extension is only available on Windows.";
    }
    return false;
#endif
}
