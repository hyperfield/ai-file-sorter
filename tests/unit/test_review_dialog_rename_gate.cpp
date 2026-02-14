#include <catch2/catch_test_macros.hpp>

#include "CategorizationDialog.hpp"
#include "TestHelpers.hpp"
#include "Types.hpp"

#include <QCheckBox>
#include <QTimer>

#ifndef _WIN32
namespace {

QCheckBox* find_checkbox_by_text(QWidget& dialog, const QString& text) {
    const auto boxes = dialog.findChildren<QCheckBox*>();
    for (auto* box : boxes) {
        if (box && box->text() == text) {
            return box;
        }
    }
    return nullptr;
}

std::vector<CategorizedFile> make_sample_files(const std::string& base_dir) {
    CategorizedFile image;
    image.file_path = base_dir;
    image.file_name = "photo.png";
    image.type = FileType::File;
    image.category = "Photos";
    image.suggested_name = "sunset.png";

    CategorizedFile document;
    document.file_path = base_dir;
    document.file_name = "report.pdf";
    document.type = FileType::File;
    document.category = "Docs";
    document.suggested_name = "report-2024.pdf";

    return {image, document};
}

} // namespace

TEST_CASE("Review dialog rename-only toggles disabled when renames are not allowed") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir base_dir;
    const auto files = make_sample_files(base_dir.path().string());

    TempDir undo_dir_disabled;
    CategorizationDialog dialog_disabled(nullptr, true, undo_dir_disabled.path().string());
    QTimer::singleShot(0, &dialog_disabled, &QDialog::accept);
    dialog_disabled.show_results(files, std::string(), false, false, false);

    QCheckBox* image_disabled = find_checkbox_by_text(
        dialog_disabled,
        QStringLiteral("Do not categorize picture files (only rename)"));
    QCheckBox* document_disabled = find_checkbox_by_text(
        dialog_disabled,
        QStringLiteral("Do not categorize document files (only rename)"));

    REQUIRE(image_disabled != nullptr);
    REQUIRE(document_disabled != nullptr);
    CHECK_FALSE(image_disabled->isEnabled());
    CHECK_FALSE(document_disabled->isEnabled());

    TempDir undo_dir_enabled;
    CategorizationDialog dialog_enabled(nullptr, true, undo_dir_enabled.path().string());
    QTimer::singleShot(0, &dialog_enabled, &QDialog::accept);
    dialog_enabled.show_results(files, std::string(), false, true, true);

    QCheckBox* image_enabled = find_checkbox_by_text(
        dialog_enabled,
        QStringLiteral("Do not categorize picture files (only rename)"));
    QCheckBox* document_enabled = find_checkbox_by_text(
        dialog_enabled,
        QStringLiteral("Do not categorize document files (only rename)"));

    REQUIRE(image_enabled != nullptr);
    REQUIRE(document_enabled != nullptr);
    CHECK(image_enabled->isEnabled());
    CHECK(document_enabled->isEnabled());
}
#endif
