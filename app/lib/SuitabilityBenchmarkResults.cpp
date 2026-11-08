#include "SuitabilityBenchmarkResults.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

namespace SuitabilityBenchmark {

void merge_result(Results& results, Result result)
{
    const auto previous = std::find_if(results.begin(), results.end(), [&](const auto& item) {
        return item.model_key == result.model_key && item.backend == result.backend
            && item.visual == result.visual;
    });
    if (previous == results.end()) results.push_back(std::move(result));
    else *previous = std::move(result);
}

Results failed_pairs(const Results& results)
{
    Results failed;
    for (const auto& result : results) if (!result.success()) failed.push_back(result);
    return failed;
}

std::string serialize_results(const Results& results)
{
    QJsonArray rows;
    for (const auto& result : results) {
        rows.append(QJsonObject{
            {"modelKey", QString::fromStdString(result.model_key)},
            {"modelPath", QString::fromStdString(result.model_path)},
            {"visualId", QString::fromStdString(result.visual_id)},
            {"label", QString::fromStdString(result.label)},
            {"backend", QString::fromStdString(result.backend)},
            {"choice", static_cast<int>(result.choice)}, {"visual", result.visual},
            {"categorizationSuccess", result.categorization_success},
            {"documentSuccess", result.document_success}, {"imageSuccess", result.image_success},
            {"categorizationSeconds", result.categorization_seconds},
            {"documentSeconds", result.document_seconds}, {"imageSeconds", result.image_seconds},
            {"detail", QString::fromStdString(result.detail)},
            {"testedAt", QString::fromStdString(result.tested_at)}});
    }
    return QJsonDocument(QJsonObject{{"version", 1}, {"results", rows}})
        .toJson(QJsonDocument::Compact).toStdString();
}

Results deserialize_results(const std::string& json)
{
    Results results;
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    const auto root = document.object();
    if (root.value("version").toInt() != 1 || !root.value("results").isArray()) return results;
    for (const auto& value : root.value("results").toArray()) {
        const auto row = value.toObject();
        Result result;
        result.model_key = row.value("modelKey").toString().toStdString();
        result.model_path = row.value("modelPath").toString().toStdString();
        result.visual_id = row.value("visualId").toString().toStdString();
        result.label = row.value("label").toString().toStdString();
        result.backend = row.value("backend").toString().toStdString();
        const int choice = row.value("choice").toInt(-1);
        if (result.model_key.empty() || result.model_path.empty() || result.label.empty()
            || choice < 0 || choice > static_cast<int>(LLMChoice::Local_4b_Qwen35)
            || (result.backend != "cpu" && result.backend != "cuda"
                && result.backend != "vulkan" && result.backend != "metal")) continue;
        result.choice = static_cast<LLMChoice>(choice);
        result.visual = row.value("visual").toBool();
        result.categorization_success = row.value("categorizationSuccess").toBool();
        result.document_success = row.value("documentSuccess").toBool();
        result.image_success = row.value("imageSuccess").toBool();
        result.categorization_seconds = row.value("categorizationSeconds").toDouble(-1);
        result.document_seconds = row.value("documentSeconds").toDouble(-1);
        result.image_seconds = row.value("imageSeconds").toDouble(-1);
        const auto valid_seconds = [](double seconds) { return std::isfinite(seconds) && seconds >= 0; };
        if (!valid_seconds(result.categorization_seconds) || !valid_seconds(result.document_seconds)
            || !valid_seconds(result.image_seconds)) continue;
        result.detail = row.value("detail").toString().toStdString();
        result.tested_at = row.value("testedAt").toString().toStdString();
        merge_result(results, std::move(result));
    }
    return results;
}

} // namespace SuitabilityBenchmark
