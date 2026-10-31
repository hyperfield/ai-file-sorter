#include <catch2/catch_test_macros.hpp>

#include "SuitabilityBenchmarkDialog.hpp"
#include "TestHelpers.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"

#include <QApplication>
#include <QTextEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <set>

namespace {
struct StorageOverrideGuard {
    std::string previous = Utils::get_llm_storage_directory_override();
    ~StorageOverrideGuard() { Utils::set_llm_storage_directory_override(previous); }
};

void write_gguf(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write("GGUF\0\0\0\0", 8);
}

struct VisualModelFiles {
    std::vector<std::unique_ptr<EnvVarGuard>> urls;

    VisualModelFiles()
    {
        for (const auto& descriptor : visual_model_descriptors()) {
            for (const auto& artifact : descriptor.artifacts) {
                urls.push_back(std::make_unique<EnvVarGuard>(artifact.url_env,
                    "https://example.com/" + std::string(descriptor.id) + "/" + artifact.local_storage_name));
            }
        }
    }

    void install(std::string_view id)
    {
        const auto* descriptor = find_visual_model_descriptor(id);
        REQUIRE(descriptor != nullptr);
        for (const auto& artifact : descriptor->artifacts) {
            write_gguf(visual_artifact_storage_path(*descriptor, artifact));
        }
    }
};

void wait_for_benchmark(SuitabilityBenchmarkDialog& dialog)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (SuitabilityBenchmarkDialogTestAccess::DialogAccess::running(dialog) && elapsed.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(1);
    }
    REQUIRE_FALSE(SuitabilityBenchmarkDialogTestAccess::DialogAccess::running(dialog));
}

void add_custom_model(Settings& settings, const std::filesystem::path& root)
{
    settings.set_llm_storage_dir((root / "models").string());
    const auto path = root / "models" / "custom.gguf";
    write_gguf(path);
    CustomLLM model;
    model.name = "Test model";
    model.path = Utils::path_to_utf8(path);
    settings.upsert_custom_llm(model);
}

SuitabilityBenchmark::Result measured(const SuitabilityBenchmark::Result& request, bool success, double seconds)
{
    auto result = request;
    result.categorization_success = success;
    result.document_success = success;
    result.image_success = success;
    result.categorization_seconds = seconds;
    result.document_seconds = seconds;
    result.image_seconds = seconds;
    if (!success) result.detail = "Simulated backend failure";
    return result;
}
} // namespace

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

TEST_CASE("Suitability benchmark never recommends failed models")
{
    QtAppContext qt;
    using namespace SuitabilityBenchmarkDialogTestAccess;
    const std::vector<MeasuredModel> results = {
        {LLMChoice::Local_4b_Qwen35, "Failed fast model", false, false, 0.01, 0.01},
        {LLMChoice::Local_7b_Gemma, "Working model", true, true, 1.0, 2.0},
    };
    const auto report = result_lines_for_testing(results, SortingMode::GeneratedCategories).join('\n');
    CHECK(report.contains(QStringLiteral("- Working model")));
    CHECK_FALSE(report.contains(QStringLiteral("- Failed fast model")));

    const auto failed = result_lines_for_testing({results.front()}, SortingMode::GeneratedCategories).join('\n');
    CHECK(failed.contains(QStringLiteral("Categorization speed: failed")));
    CHECK(failed.contains(QStringLiteral("Document analysis speed: failed")));
    CHECK(failed.contains(QStringLiteral("- n/a")));
    CHECK_FALSE(failed.contains(QStringLiteral("optimal")));

    const auto partial = result_lines_for_testing(
        {{LLMChoice::Local_4b_Qwen35, "Partial model", true, false, 0.1, 0.1}},
        SortingMode::GeneratedCategories).join('\n');
    CHECK(partial.contains(QStringLiteral("Document analysis speed: failed")));
    CHECK_FALSE(partial.contains(QStringLiteral("- Partial model")));
}

TEST_CASE("Suitability benchmark recommendations respect existing folder sorting")
{
    QtAppContext qt;
    using namespace SuitabilityBenchmarkDialogTestAccess;
    const std::vector<MeasuredModel> results = {
        {LLMChoice::Local_4b_Qwen35, "Qwen candidate", true, true, 1.0, 2.0},
        {LLMChoice::Local_4b_Gemma, "Fast Gemma 4B", true, true, 0.1, 0.1},
        {LLMChoice::Custom, "Fast custom model", true, true, 0.1, 0.1},
        {LLMChoice::Local_7b_Gemma, "Gemma 7B candidate", true, true, 2.0, 3.0},
    };
    const auto generated = result_lines_for_testing(results, SortingMode::GeneratedCategories).join('\n');
    CHECK(generated.contains(QStringLiteral("- Fast Gemma 4B")));
    CHECK(generated.contains(QStringLiteral("- Fast custom model")));
    const auto existing = result_lines_for_testing(results, SortingMode::ExistingFolderTree).join('\n');
    CHECK(existing.contains(QStringLiteral("- Qwen candidate")));
    CHECK(existing.contains(QStringLiteral("- Gemma 7B candidate")));
    CHECK_FALSE(existing.contains(QStringLiteral("- Fast Gemma 4B")));
    CHECK_FALSE(existing.contains(QStringLiteral("- Fast custom model")));
    CHECK(existing.contains(QStringLiteral("do not measure folder-routing accuracy")));

    const auto unsuitable_only = result_lines_for_testing({results[1]}, SortingMode::ExistingFolderTree).join('\n');
    CHECK(unsuitable_only.contains(QStringLiteral("- n/a")));
    CHECK(unsuitable_only.contains(QStringLiteral("recommend Qwen3.5 4B")));
}

TEST_CASE("Suitability benchmark detects custom text models and rejects invalid or duplicate files")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    settings.set_llm_storage_dir((temp.path() / "models").string());
    CHECK_FALSE(SuitabilityBenchmarkDialog::models_available(settings));
    const auto model = temp.path() / "models" / "custom.gguf";
    const auto corrupt = temp.path() / "models" / "corrupt.gguf";
    write_gguf(model);
    std::ofstream(corrupt).put('x');
    CustomLLM valid;
    valid.id = "valid";
    valid.name = "Valid custom";
    valid.path = Utils::path_to_utf8(model);
    CustomLLM duplicate = valid;
    duplicate.id = "duplicate";
    duplicate.path = Utils::path_to_utf8(model.parent_path() / "." / model.filename());
    CustomLLM invalid = valid;
    invalid.id = "corrupt";
    invalid.path = Utils::path_to_utf8(corrupt);
    CustomLLM missing = valid;
    missing.id = "missing";
    missing.path = Utils::path_to_utf8(temp.path() / "missing.gguf");
    settings.upsert_custom_llm(valid);
    const auto paths = SuitabilityBenchmarkDialogTestAccess::local_model_paths_for_testing(
        {valid, duplicate, invalid, missing});
    REQUIRE(paths.size() == 1);
    CHECK(paths.front() == Utils::path_to_utf8(model));
    CHECK(SuitabilityBenchmarkDialog::models_available(settings));
}

TEST_CASE("Suitability benchmark discovers all installed visual models")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    VisualModelFiles files;
    for (const auto& descriptor : visual_model_descriptors()) files.install(descriptor.id);

    std::set<std::string> expected;
    for (const auto& descriptor : visual_model_descriptors()) expected.insert(descriptor.id);
    for (const auto* name : {"First visual model", "Second visual model"}) {
        CustomLLM custom;
        custom.name = name;
        custom.path = Utils::path_to_utf8(temp.path() / name / "model.gguf");
        custom.mmproj_path = Utils::path_to_utf8(temp.path() / name / "mmproj.gguf");
        write_gguf(Utils::utf8_to_path(custom.path));
        write_gguf(Utils::utf8_to_path(custom.mmproj_path));
        expected.insert(custom_visual_model_id_for_llm(settings.upsert_custom_llm(custom)));
    }
    CustomLLM incomplete;
    incomplete.name = "Missing projector";
    incomplete.path = settings.get_custom_llms().front().path;
    incomplete.mmproj_path = Utils::path_to_utf8(temp.path() / "missing.gguf");
    settings.upsert_custom_llm(incomplete);
    incomplete.id.clear();
    incomplete.name = "Corrupt projector";
    incomplete.mmproj_path = Utils::path_to_utf8(temp.path() / "corrupt.gguf");
    std::ofstream(Utils::utf8_to_path(incomplete.mmproj_path)).put('x');
    settings.upsert_custom_llm(incomplete);
    settings.set_visual_model_id("gemma-3-4b-it");

    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [](const auto& request) { return measured(request, true, 1.0); }, {"cpu"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("cpu")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    std::set<std::string> discovered;
    std::set<std::string> labels;
    for (const auto& result : Access::results(dialog)) {
        if (!result.visual) continue;
        discovered.insert(result.visual_id);
        labels.insert(result.label);
        CHECK(result.success());
        CHECK(result.backend == "cpu");
    }
    CHECK(discovered == expected);
    CHECK(labels.size() == expected.size());
    CHECK(labels.contains("Image analysis: Custom: First visual model"));
    CHECK(labels.contains("Image analysis: Custom: Second visual model"));
    CHECK(settings.get_visual_model_id() == "gemma-3-4b-it");

    Settings restored;
    REQUIRE(restored.load());
    SuitabilityBenchmarkDialog reopened(restored);
    std::set<std::string> persisted;
    for (const auto& result : Access::results(reopened)) {
        if (result.visual) persisted.insert(result.visual_id);
    }
    CHECK(persisted == expected);
}

TEST_CASE("Suitability benchmark detects unselected visual models without text models")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    settings.set_llm_storage_dir(Utils::path_to_utf8(temp.path() / "models"));
    settings.set_visual_model_id("qwen3.5-4b");
    VisualModelFiles files;
    CHECK_FALSE(SuitabilityBenchmarkDialog::models_available(settings));
    files.install("llava-v1.6-mistral-7b");
    const auto* descriptor = find_visual_model_descriptor("llava-v1.6-mistral-7b");
    REQUIRE(descriptor != nullptr);
    const auto projector = visual_artifact_storage_path(*descriptor, descriptor->artifacts[1]);
    CHECK(SuitabilityBenchmarkDialogTestAccess::local_model_paths_for_testing({}).empty());
    CHECK(SuitabilityBenchmarkDialog::models_available(settings));
    REQUIRE(std::filesystem::remove(projector));
    CHECK_FALSE(SuitabilityBenchmarkDialog::models_available(settings));
    std::ofstream(projector).put('x');
    CHECK_FALSE(SuitabilityBenchmarkDialog::models_available(settings));
    write_gguf(projector);

    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [](const auto& request) { return measured(request, true, 1.0); }, {"cpu"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("cpu")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    const auto results = Access::results(dialog);
    REQUIRE(results.size() == 1);
    CHECK(results.front().visual);
    CHECK(results.front().visual_id == "llava-v1.6-mistral-7b");
    CHECK(results.front().success());
    CHECK(settings.get_visual_model_id() == "qwen3.5-4b");
}

TEST_CASE("Suitability benchmark gives Qwen explicit four billion parameter speed targets")
{
    using namespace SuitabilityBenchmarkDialogTestAccess;
    EnvVarGuard cat_optimal("BENCH_CAT_LOCAL_QWEN35_4B_OPTIMAL_MAX", std::nullopt);
    EnvVarGuard cat_acceptable("BENCH_CAT_LOCAL_QWEN35_4B_ACCEPTABLE_MAX", std::nullopt);
    EnvVarGuard doc_optimal("BENCH_DOC_LOCAL_QWEN35_4B_OPTIMAL_MAX", std::nullopt);
    EnvVarGuard doc_acceptable("BENCH_DOC_LOCAL_QWEN35_4B_ACCEPTABLE_MAX", std::nullopt);
    CHECK(thresholds_for_testing(LLMChoice::Local_4b_Qwen35, false) == std::make_pair(2.0, 5.0));
    CHECK(thresholds_for_testing(LLMChoice::Local_4b_Qwen35, true) == std::make_pair(4.0, 7.0));
    {
        EnvVarGuard override_value("BENCH_CAT_LOCAL_QWEN35_4B_OPTIMAL_MAX", std::string("3.5"));
        CHECK(thresholds_for_testing(LLMChoice::Local_4b_Qwen35, false).first == 3.5);
    }
    for (const auto* value : {"nan", "inf", "-2", "0", "2junk"}) {
        EnvVarGuard invalid_value("BENCH_CAT_LOCAL_QWEN35_4B_OPTIMAL_MAX", std::string(value));
        CHECK(thresholds_for_testing(LLMChoice::Local_4b_Qwen35, false).first == 2.0);
    }
}

TEST_CASE("Suitability benchmark only marks completed checks as completed")
{
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    SuitabilityBenchmarkDialog dialog(settings);
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::finish(dialog, false);
    CHECK_FALSE(settings.get_suitability_benchmark_completed());
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::finish(dialog, true);
    CHECK(settings.get_suitability_benchmark_completed());
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::finish(dialog, false);
    CHECK(settings.get_suitability_benchmark_completed());
    Settings restored;
    REQUIRE(restored.load());
    CHECK(restored.get_suitability_benchmark_completed());
}

TEST_CASE("Suitability benchmark Escape requests stop without hiding a running check")
{
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    SuitabilityBenchmarkDialog dialog(settings);
    dialog.show();
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::set_running(dialog, true);
    dialog.reject();
    CHECK_FALSE(dialog.isHidden());
    auto* output = dialog.findChild<QTextEdit*>(QStringLiteral("benchmarkActivityView"));
    REQUIRE(output != nullptr);
    CHECK(output->toPlainText().contains(QStringLiteral("[STOP]")));
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::set_running(dialog, false);
    dialog.reject();
    CHECK(dialog.isHidden());
}

TEST_CASE("Suitability benchmark respects the shared analysis runtime lock")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    settings.set_llm_storage_dir((temp.path() / "models").string());
    AnalysisRuntimeLock lock(Utils::utf8_to_path(settings.get_config_dir()) / "runtime");
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Headless;
    auto lease = lock.try_acquire(metadata);
    REQUIRE(lease.has_value());
    SuitabilityBenchmarkDialog dialog(settings);
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::start(dialog);
    auto* output = dialog.findChild<QTextEdit*>(QStringLiteral("benchmarkActivityView"));
    REQUIRE(output != nullptr);
    CHECK(output->toPlainText().contains(QStringLiteral("runtime is busy")));
    CHECK(lease->owns_lock());
    CHECK_FALSE(settings.get_suitability_benchmark_completed());
    lease->release();
    SuitabilityBenchmarkDialogTestAccess::DialogAccess::start(dialog);
    CHECK(output->toPlainText().contains(QStringLiteral("No usable local LLM files")));
    CHECK_FALSE(lock.is_locked());
}

TEST_CASE("Suitability benchmark tracks and persists model backend pairs independently")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    std::vector<std::string> attempts;
    Access::set_runner(dialog, [&](const auto& request) {
        attempts.push_back(request.backend);
        return measured(request, request.backend == "cpu", 1.5);
    }, {"cpu", "cuda", "vulkan"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("all")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    CHECK(attempts == std::vector<std::string>{"cuda", "vulkan", "cpu"});
    const auto results = Access::results(dialog);
    REQUIRE(results.size() == 3);
    CHECK_FALSE(results[0].success());
    CHECK_FALSE(results[1].success());
    CHECK(results[2].success());
    const auto summary = Access::summary(dialog).join('\n');
    CHECK(summary.contains(QStringLiteral("[CPU]")));
    CHECK_FALSE(summary.contains(QStringLiteral("[CUDA]")));
    CHECK_FALSE(summary.contains(QStringLiteral("[Vulkan]")));
    CHECK(settings.get_llm_choice() == LLMChoice::Unset);
    Settings restored;
    REQUIRE(restored.load());
    SuitabilityBenchmarkDialog reopened(restored);
    REQUIRE(Access::results(reopened).size() == 3);
    CHECK(Access::results(reopened)[2].success());
    auto* retry = reopened.findChild<QPushButton*>(QStringLiteral("benchmarkRetryFailed"));
    REQUIRE(retry != nullptr);
    CHECK(retry->isEnabled());
}

TEST_CASE("Suitability benchmark retries only failed pairs and preserves successful backends")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [](const auto& request) {
        return measured(request, request.backend == "cpu", 1.5);
    }, {"cpu", "cuda"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("all")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    const auto before = Access::results(dialog);
    REQUIRE(before.size() == 2);
    std::vector<std::string> retried;
    Access::set_runner(dialog, [&](const auto& request) {
        retried.push_back(request.backend);
        return measured(request, true, 0.25);
    }, {"cpu", "cuda"});
    auto* retry = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetryFailed"));
    REQUIRE(retry != nullptr);
    REQUIRE(retry->isEnabled());
    retry->click();
    wait_for_benchmark(dialog);
    CHECK(retried == std::vector<std::string>{"cuda"});
    const auto after = Access::results(dialog);
    REQUIRE(after.size() == 2);
    CHECK(after[0].success());
    CHECK(after[1].success());
    CHECK(after[1].categorization_seconds == before[1].categorization_seconds);
    CHECK(after[1].tested_at == before[1].tested_at);
    CHECK_FALSE(retry->isEnabled());
    CHECK(Access::summary(dialog).join('\n').contains(QStringLiteral("[CUDA]")));
}

TEST_CASE("Suitability benchmark can try a selected model on another backend")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [](const auto& request) { return measured(request, true, 1.0); },
                       {"cpu", "vulkan"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("cpu")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("benchmarkResultsTable"));
    REQUIRE(table != nullptr);
    table->selectRow(0);
    backend->setCurrentIndex(backend->findData(QStringLiteral("vulkan")));
    std::vector<std::string> attempts;
    Access::set_runner(dialog, [&](const auto& request) {
        attempts.push_back(request.backend);
        return measured(request, false, 0.1);
    }, {"cpu", "vulkan"});
    auto* try_backend = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkTryBackend"));
    REQUIRE(try_backend != nullptr);
    REQUIRE(try_backend->isEnabled());
    try_backend->click();
    wait_for_benchmark(dialog);
    CHECK(attempts == std::vector<std::string>{"vulkan"});
    auto results = Access::results(dialog);
    REQUIRE(results.size() == 2);
    CHECK(results[0].backend == "cpu");
    CHECK(results[0].success());
    CHECK(results[1].backend == "vulkan");
    CHECK_FALSE(results[1].success());
    CHECK(Access::summary(dialog).join('\n').contains(QStringLiteral("[CPU]")));

    table->selectRow(1);
    auto* retry = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetrySelected"));
    REQUIRE(retry != nullptr);
    attempts.clear();
    Access::set_runner(dialog, [&](const auto& request) {
        attempts.push_back(request.backend);
        return measured(request, true, 0.5);
    }, {"cpu", "vulkan"});
    retry->click();
    wait_for_benchmark(dialog);
    CHECK(attempts == std::vector<std::string>{"vulkan"});
    CHECK(Access::results(dialog)[0].categorization_seconds == 1.0);
    CHECK(Access::results(dialog)[1].success());
}

TEST_CASE("Suitability benchmark automatic CPU fallback records a separate backend result")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard gpu_override("AI_FILE_SORTER_GPU_BACKEND", std::string("cuda"));
    EnvVarGuard visual_override("AI_FILE_SORTER_VISUAL_USE_GPU", std::string("1"));
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    std::vector<std::string> attempts;
    Access::set_runner(dialog, [&](const auto& request) {
        attempts.push_back(request.backend);
        return measured(request, request.backend == "cpu", 0.5);
    }, {"cpu", "cuda"});
    Access::start(dialog);
    wait_for_benchmark(dialog);
    CHECK(attempts == std::vector<std::string>{"cuda", "cpu"});
    const auto results = Access::results(dialog);
    REQUIRE(results.size() == 2);
    CHECK_FALSE(results[0].success());
    CHECK(results[1].success());
    CHECK(std::string(std::getenv("AI_FILE_SORTER_GPU_BACKEND")) == "cuda");
    CHECK(std::string(std::getenv("AI_FILE_SORTER_VISUAL_USE_GPU")) == "1");
}

TEST_CASE("Suitability benchmark cancellation preserves existing pair measurements")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [](const auto& request) { return measured(request, true, 1.0); }, {"cpu"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("cpu")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    const auto before = Access::results(dialog);
    std::promise<void> stopped;
    auto stop_processed = stopped.get_future();
    Access::set_runner(dialog, [&](const auto& request) {
        QMetaObject::invokeMethod(&dialog, [&]() { dialog.reject(); stopped.set_value(); }, Qt::QueuedConnection);
        stop_processed.wait_for(std::chrono::seconds(1));
        return measured(request, false, 0.1);
    }, {"cpu"});
    auto* retry = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetrySelected"));
    REQUIRE(retry != nullptr);
    retry->click();
    wait_for_benchmark(dialog);
    const auto after = Access::results(dialog);
    REQUIRE(after.size() == before.size());
    CHECK(after[0].success());
    CHECK(after[0].categorization_seconds == before[0].categorization_seconds);
    CHECK(after[0].tested_at == before[0].tested_at);
}

TEST_CASE("Suitability benchmark result storage validates data and replaces only matching pairs")
{
    SuitabilityBenchmark::Result cpu;
    cpu.model_key = "model-1";
    cpu.model_path = "/models/example.gguf";
    cpu.label = "Example";
    cpu.backend = "cpu";
    cpu.choice = LLMChoice::Local_4b_Qwen35;
    cpu = measured(cpu, true, 1.0);
    auto cuda = cpu;
    cuda.backend = "cuda";
    cuda = measured(cuda, false, 0.1);
    SuitabilityBenchmark::Results results{cpu, cuda};
    SuitabilityBenchmark::merge_result(results, measured(cuda, true, 0.5));
    REQUIRE(results.size() == 2);
    CHECK(results[0].categorization_seconds == 1.0);
    CHECK(results[1].categorization_seconds == 0.5);
    const auto restored = SuitabilityBenchmark::deserialize_results(
        SuitabilityBenchmark::serialize_results(results));
    REQUIRE(restored.size() == 2);
    CHECK(restored[0].backend == "cpu");
    CHECK(restored[1].backend == "cuda");
    CHECK(restored[0].success());
    CHECK(restored[1].success());
    CHECK(SuitabilityBenchmark::deserialize_results("not-json").empty());
    CHECK(SuitabilityBenchmark::deserialize_results("{\"version\":99,\"results\":[]}").empty());
    auto invalid = cpu;
    invalid.backend = "unknown";
    CHECK(SuitabilityBenchmark::deserialize_results(SuitabilityBenchmark::serialize_results({invalid})).empty());
    invalid = cpu;
    invalid.categorization_seconds = -1;
    CHECK(SuitabilityBenchmark::deserialize_results(SuitabilityBenchmark::serialize_results({invalid})).empty());
}

TEST_CASE("Suitability benchmark retries visual model backend pairs without rerunning text checks")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    auto custom = settings.get_custom_llms().front();
    const auto projector = temp.path() / "models" / "mmproj.gguf";
    write_gguf(projector);
    custom.mmproj_path = Utils::path_to_utf8(projector);
    settings.upsert_custom_llm(custom);
    const auto first_visual_id = custom_visual_model_id_for_llm(custom.id);
    auto second = custom;
    second.id.clear();
    second.name = "Another visual model";
    second.path = Utils::path_to_utf8(temp.path() / "models" / "another.gguf");
    write_gguf(Utils::utf8_to_path(second.path));
    const auto second_visual_id = custom_visual_model_id_for_llm(settings.upsert_custom_llm(second));
    settings.set_visual_model_id(first_visual_id);
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    Access::set_runner(dialog, [&](const auto& request) {
        return measured(request, request.visual_id != first_visual_id || request.backend == "cpu", 1.0);
    }, {"cpu", "cuda"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("all")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    const auto before = Access::results(dialog);
    REQUIRE(before.size() == 8);
    // Changing the selected visual model must not discard another model's retry or measurements.
    settings.set_visual_model_id(second_visual_id);
    std::vector<SuitabilityBenchmark::Result> retried;
    Access::set_runner(dialog, [&](const auto& request) {
        retried.push_back(request);
        return measured(request, true, 0.5);
    }, {"cpu", "cuda"});
    auto* retry = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetryFailed"));
    REQUIRE(retry != nullptr);
    retry->click();
    wait_for_benchmark(dialog);
    REQUIRE(retried.size() == 1);
    CHECK(retried[0].visual);
    CHECK(retried[0].visual_id == first_visual_id);
    CHECK(retried[0].backend == "cuda");
    const auto results = Access::results(dialog);
    REQUIRE(results.size() == before.size());
    CHECK(std::all_of(results.begin(), results.end(), [](const auto& result) { return result.success(); }));
    for (size_t row = 0; row < before.size(); ++row) {
        if (!before[row].success()) continue;
        CHECK(results[row].model_key == before[row].model_key);
        CHECK(results[row].image_seconds == before[row].image_seconds);
        CHECK(results[row].categorization_seconds == before[row].categorization_seconds);
        CHECK(results[row].tested_at == before[row].tested_at);
    }

    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("benchmarkResultsTable"));
    REQUIRE(table != nullptr);
    const auto first_visual = std::find_if(results.begin(), results.end(), [&](const auto& result) {
        return result.visual_id == first_visual_id && result.backend == "cpu";
    });
    REQUIRE(first_visual != results.end());
    table->selectRow(static_cast<int>(std::distance(results.begin(), first_visual)));
    retried.clear();
    auto* retry_selected = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetrySelected"));
    REQUIRE(retry_selected != nullptr);
    retry_selected->click();
    wait_for_benchmark(dialog);
    REQUIRE(retried.size() == 1);
    CHECK(retried[0].visual_id == first_visual_id);
    CHECK(retried[0].backend == "cpu");
    CHECK(Access::results(dialog).size() == before.size());
    CHECK(settings.get_visual_model_id() == second_visual_id);
}

TEST_CASE("Suitability benchmark invalidates measurements when model weights change")
{
    QtAppContext qt;
    StorageOverrideGuard storage;
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    add_custom_model(settings, temp.path());
    SuitabilityBenchmarkDialog dialog(settings);
    using Access = SuitabilityBenchmarkDialogTestAccess::DialogAccess;
    int attempts = 0;
    Access::set_runner(dialog, [&](const auto& request) {
        ++attempts;
        return measured(request, true, 1.0);
    }, {"cpu"});
    auto* backend = dialog.findChild<QComboBox*>(QStringLiteral("benchmarkBackendCombo"));
    REQUIRE(backend != nullptr);
    backend->setCurrentIndex(backend->findData(QStringLiteral("cpu")));
    Access::start(dialog);
    wait_for_benchmark(dialog);
    REQUIRE(Access::results(dialog).size() == 1);
    std::ofstream(temp.path() / "models" / "custom.gguf", std::ios::app).put('x');
    auto* retry = dialog.findChild<QPushButton*>(QStringLiteral("benchmarkRetrySelected"));
    REQUIRE(retry != nullptr);
    retry->click();
    wait_for_benchmark(dialog);
    CHECK(attempts == 1);
    CHECK(Access::results(dialog).empty());
    auto* output = dialog.findChild<QTextEdit*>(QStringLiteral("benchmarkActivityView"));
    REQUIRE(output != nullptr);
    CHECK(output->toPlainText().contains(QStringLiteral("model files changed")));
}
