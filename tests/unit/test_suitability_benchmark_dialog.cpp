#include <catch2/catch_test_macros.hpp>

#include "SuitabilityBenchmarkDialog.hpp"
#include "TestHelpers.hpp"

TEST_CASE("Suitability benchmark recognizes Metal backend aliases")
{
    QtAppContext qt_context;
    CHECK(SuitabilityBenchmarkDialogTestAccess::backend_alias_matches("MTL", "metal"));
    CHECK(SuitabilityBenchmarkDialogTestAccess::backend_alias_matches("MTL0", "metal"));
    CHECK(SuitabilityBenchmarkDialogTestAccess::backend_alias_matches("Metal", "mtl"));
    CHECK(SuitabilityBenchmarkDialogTestAccess::backend_alias_matches("mtl", "Metal"));
    CHECK_FALSE(SuitabilityBenchmarkDialogTestAccess::backend_alias_matches("Vulkan", "metal"));

    const QString text_note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("MTL", false, false, true, false);
    CHECK(text_note == QStringLiteral("GPU (target: Metal)"));

    const QString image_note =
        SuitabilityBenchmarkDialogTestAccess::image_backend_note_for_testing("mtl", true, false, false, true);
    CHECK(image_note == QStringLiteral("GPU (target: Metal)"));

    const QString fallback_note =
        SuitabilityBenchmarkDialogTestAccess::image_backend_note_for_testing("MTL", true, false, false, false);
    CHECK(fallback_note.startsWith(QStringLiteral("CPU")));
    CHECK(fallback_note.contains(QStringLiteral("GPU via Metal unavailable")));
}

TEST_CASE("Suitability benchmark reports Vulkan GPU when CUDA is unavailable")
{
    QtAppContext qt_context;
    const QString text_note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("", false, true, false, false);
    CHECK(text_note == QStringLiteral("GPU (target: Vulkan)"));

    const QString image_note =
        SuitabilityBenchmarkDialogTestAccess::image_backend_note_for_testing("", true, false, true, false);
    CHECK(image_note == QStringLiteral("GPU (target: Vulkan)"));
}

TEST_CASE("Suitability benchmark reports CPU only when no auto GPU backend is available")
{
    QtAppContext qt_context;
    const QString note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("", false, false, false, false);

    CHECK(note.startsWith(QStringLiteral("CPU")));
    CHECK(note.contains(QStringLiteral("GPU init failed")));
}

TEST_CASE("Suitability benchmark keeps explicit CUDA unavailability distinct from Vulkan")
{
    QtAppContext qt_context;
    const QString note =
        SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing("cuda", false, true, false, true);

    CHECK(note.startsWith(QStringLiteral("CPU")));
    CHECK(note.contains(QStringLiteral("GPU via CUDA unavailable")));
    CHECK_FALSE(note.contains(QStringLiteral("Vulkan unavailable")));
}
