#pragma once

namespace UpdaterBuildConfig {

enum class Mode {
    AutoInstall,
    NotifyOnly,
    Disabled,
};

constexpr Mode current_mode()
{
#if defined(AI_FILE_SORTER_UPDATE_MODE_DISABLED)
    return Mode::Disabled;
#elif defined(AI_FILE_SORTER_UPDATE_MODE_NOTIFY_ONLY)
    return Mode::NotifyOnly;
#elif defined(AI_FILE_SORTER_UPDATE_MODE_AUTO_INSTALL)
    return Mode::AutoInstall;
#else
    return Mode::AutoInstall;
#endif
}

constexpr bool update_checks_enabled()
{
    return current_mode() != Mode::Disabled;
}

constexpr bool auto_install_enabled()
{
    return current_mode() == Mode::AutoInstall;
}

} // namespace UpdaterBuildConfig
