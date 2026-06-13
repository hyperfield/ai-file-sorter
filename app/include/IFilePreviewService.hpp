/**
 * @file IFilePreviewService.hpp
 * @brief Preview-service abstraction for file review flows.
 */
#pragma once

#include <filesystem>
#include <memory>

class QWidget;

/**
 * @brief Shows a file preview using the best platform-specific mechanism available.
 *
 * Implementations may embed a native preview experience inside the application or
 * fall back to opening the file in the platform default application when embedded
 * preview is unavailable for the selected file type.
 */
class IFilePreviewService {
public:
    virtual ~IFilePreviewService() = default;

    /**
     * @brief Preview a file path for the user.
     * @param file_path Absolute or relative file path to preview.
     * @param parent Parent widget used for modal preview surfaces when needed.
     * @return True when the preview action was launched successfully.
     */
    virtual bool preview_file(const std::filesystem::path& file_path,
                              QWidget* parent) = 0;
};

/**
 * @brief Create the default preview service for the current platform.
 * @return Preview service instance owned by the caller.
 */
std::unique_ptr<IFilePreviewService> create_file_preview_service();
