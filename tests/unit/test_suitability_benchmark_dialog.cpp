#include <catch2/catch_test_macros.hpp>

#include "SuitabilityBenchmarkDialog.hpp"

TEST_CASE("Suitability benchmark reports Vulkan GPU when CUDA is unavailable")
{
    const QString text_note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("", false, true, false, false);
    CHECK(text_note == QStringLiteral("GPU (target: Vulkan)"));

    const QString image_note =
        SuitabilityBenchmarkDialogTestAccess::image_backend_note_for_testing("", true, false, true, false);
    CHECK(image_note == QStringLiteral("GPU (target: Vulkan)"));
}

TEST_CASE("Suitability benchmark reports CPU only when no auto GPU backend is available")
{
    const QString note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("", false, false, false, false);

    CHECK(note.startsWith(QStringLiteral("CPU")));
    CHECK(note.contains(QStringLiteral("GPU init failed")));
}

TEST_CASE("Suitability benchmark keeps explicit CUDA unavailability distinct from Vulkan")
{
    const QString note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("cuda", false, true, false, true);

    CHECK(note.startsWith(QStringLiteral("CPU")));
    CHECK(note.contains(QStringLiteral("GPU via CUDA unavailable")));
    CHECK_FALSE(note.contains(QStringLiteral("Vulkan unavailable")));
}
