#pragma once

#include "OneDrivePlatform.hpp"

namespace one_drive {

bool is_retryable_macos_error(int error);
PathInspection inspect_macos_path(const std::filesystem::path &path);

} // namespace one_drive
