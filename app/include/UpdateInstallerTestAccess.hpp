#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "UpdateInstaller.hpp"

class UpdateInstallerTestAccess {
public:
    static UpdateInstaller::LaunchRequest build_launch_request(const std::filesystem::path& installer_path);
};

#endif // AI_FILE_SORTER_TEST_BUILD
