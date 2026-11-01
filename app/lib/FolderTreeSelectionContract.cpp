#include "FolderTreeSelectionContract.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>

namespace {
std::optional<QJsonObject> response_object(const std::string& response, bool create)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(response), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto object = document.object();
    if (object.size() != 2 || !object.contains("targetFolder")) return {};
    const auto flag = object.value("createFolder");
    if (!flag.isBool() || flag.toBool() != create) return {};
    return object;
}

LLMResponseConstraint schema(QJsonObject target, bool create)
{
    const QJsonObject object{{"type", "object"}, {"additionalProperties", false},
        {"required", QJsonArray{"targetFolder", "createFolder"}},
        {"properties", QJsonObject{{"targetFolder", target},
            {"createFolder", QJsonObject{{"const", create}}}}}};
    return {QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString()};
}
}

LLMResponseConstraint FolderTreeSelectionContract::existing_constraint(
    std::span<const FolderTreeCatalog::Entry> entries)
{
    QJsonArray paths;
    for (const auto& entry : entries) paths.append(QString::fromStdString(entry.relative_path));
    paths.append(QJsonValue::Null);
    return schema(QJsonObject{{"enum", paths}}, false);
}

LLMResponseConstraint FolderTreeSelectionContract::proposal_constraint()
{
    return schema(QJsonObject{{"type", "string"}, {"minLength", 1}}, true);
}

FolderTreeSelectionContract::Decision FolderTreeSelectionContract::parse_existing(
    const std::string& response, const FolderTreeCatalog::Catalog& candidates)
{
    const auto object = response_object(response, false);
    if (!object) return {};
    const auto target = object->value("targetFolder");
    if (target.isNull()) return {Outcome::NoMatch, {}};
    if (!target.isString()) return {};
    const auto path = target.toString().toStdString();
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
    if (!validation.valid || validation.normalized_path != path) return {};
    const auto& entries = candidates.entries();
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.relative_path == path;
    });
    if (found == entries.end()) return {};
    return {Outcome::Selected, FolderTreeCatalog::Selection{path, false, true}};
}

std::optional<FolderTreeCatalog::Selection> FolderTreeSelectionContract::parse_proposal(
    const std::string& response, const FolderTreeCatalog::Catalog& catalog)
{
    const auto object = response_object(response, true);
    if (!object || !object->value("targetFolder").isString()) return {};
    const auto selection = FolderTreeCatalog::parse_selection(response, catalog, true);
    if (!selection) return {};
    // The catalog, not the model's creation flag, decides whether a literal target already exists.
    const auto target = object->value("targetFolder").toString().toStdString();
    if (selection->exists && selection->relative_path != target) return {};
    return selection;
}

std::string_view FolderTreeSelectionContract::proposal_instruction()
{
    return "\n[[AI_FILE_SORTER_NEW_FOLDER_PROPOSAL]]\n"
        "The existing-folder comparison explicitly found no suitable destination. "
        "Now propose one concise new relative folder path, following the listed parent names and conventions. "
        "Return exactly one JSON object with targetFolder as a nonempty string and createFolder:true.\n";
}

std::string_view FolderTreeSelectionContract::purpose_comparison_instruction()
{
    return "\nPurpose-aware comparison:\n"
        "Use any folder-purpose descriptions to interpret the archive's organization. "
        "Consider the file's type, use, and subject where the evidence describes them. "
        "Preliminary labels are supporting clues; unstated details remain unknown.\n";
}
