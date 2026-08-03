#include "ExplorerExtensionEntitlement.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::wstring_view kExpectedProduct = L"ExplorerExtension";

bool equals_case_insensitive(std::wstring_view left, std::wstring_view right)
{
    return left.size() == right.size() &&
           std::equal(left.cbegin(), left.cend(), right.cbegin(), [](wchar_t a, wchar_t b) {
               return towlower(a) == towlower(b);
           });
}

bool paid_state(std::wstring_view state)
{
    return equals_case_insensitive(state, L"Paid") ||
           equals_case_insensitive(state, L"Owned") ||
           equals_case_insensitive(state, L"Active");
}

bool truthy_env_value(const char* value)
{
    if (!value) {
        return false;
    }
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "YES";
}

#ifdef _WIN32

constexpr std::wstring_view kSettingsKey = L"Software\\HFStudio\\AIFileSorter\\ExplorerExtension";
constexpr std::wstring_view kLegacySettingsKey = L"Software\\Quicknode\\AIFileSorter\\ExplorerExtension";
constexpr std::wstring_view kEntitlementStateValue = L"EntitlementState";
constexpr std::wstring_view kEntitlementProductValue = L"EntitlementProduct";
constexpr std::wstring_view kExtensionPathValue = L"ExtensionPath";
constexpr std::wstring_view kProgressPathValue = L"ProgressPath";
constexpr std::wstring_view kPackageRepositoryKey =
    L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
constexpr std::wstring_view kPaidPackageIdentityPrefix = L"Hyperfield.AIFileSorterforFileExplorer_";

bool existing_file_path(std::wstring_view value)
{
    if (value.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(value), ec) && !ec;
}

std::optional<std::wstring> read_registry_string(HKEY root,
                                                 std::wstring_view subkey,
                                                 std::wstring_view value_name)
{
    HKEY key = nullptr;
    const std::wstring subkey_copy(subkey);
    LSTATUS status = RegOpenKeyExW(root, subkey_copy.c_str(), 0, KEY_READ, &key);
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
    return value;
}

bool paid_entitlement_at(std::wstring_view subkey)
{
    const auto state = read_registry_string(HKEY_CURRENT_USER, subkey, kEntitlementStateValue);
    if (!state || !paid_state(*state)) {
        return false;
    }

    const auto product = read_registry_string(HKEY_CURRENT_USER, subkey, kEntitlementProductValue);
    if (!product || !equals_case_insensitive(*product, kExpectedProduct)) {
        return false;
    }

    const auto extension_path = read_registry_string(HKEY_CURRENT_USER, subkey, kExtensionPathValue);
    const auto progress_path = read_registry_string(HKEY_CURRENT_USER, subkey, kProgressPathValue);
    return (extension_path && existing_file_path(*extension_path)) ||
           (progress_path && existing_file_path(*progress_path));
}

bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix)
{
    return value.size() >= prefix.size() &&
           std::equal(prefix.cbegin(), prefix.cend(), value.cbegin(), [](wchar_t a, wchar_t b) {
               return towlower(a) == towlower(b);
           });
}

bool paid_msix_package_is_registered()
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
            starts_with_case_insensitive(std::wstring_view(name.data(), length), kPaidPackageIdentityPrefix)) {
            RegCloseKey(key);
            return true;
        }
    }

    RegCloseKey(key);
    return false;
}

#endif

} // namespace

bool ExplorerExtensionEntitlement::has_paid_entitlement()
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (const char* override_value = std::getenv("AI_FILE_SORTER_TEST_PAID_EXPLORER_EXTENSION")) {
        return truthy_env_value(override_value);
    }
#endif

#ifdef _WIN32
    return paid_entitlement_at(kSettingsKey) ||
           paid_entitlement_at(kLegacySettingsKey) ||
           paid_msix_package_is_registered();
#else
    return false;
#endif
}
