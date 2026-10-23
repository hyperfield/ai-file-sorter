#include "FolderPurposeMetadata.hpp"
#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringDecoder>
#include <optional>

namespace {
constexpr std::size_t kMaxDocumentBytes = 128 * 1024;
constexpr qsizetype kMaxDescriptionBytes = 768;
constexpr qsizetype kMaxFolderDescriptions = 500;
constexpr int kMetadataVersion = 1;

FolderPurposeMetadata::Result failure(const char* error)
{
    return {{}, error};
}

std::optional<std::string> parse_description(const std::string& path, const QJsonValue& value)
{
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(path);
    if (!validation.valid || validation.normalized_path != path || !value.isString()) return {};
    const auto text = value.toString().trimmed().toUtf8();
    if (text.isEmpty() || text.size() > kMaxDescriptionBytes || text.contains('\0')) return {};
    return text.toStdString();
}
}

FolderPurposeMetadata::Result FolderPurposeMetadata::parse(std::string_view document)
{
    if (document.size() > kMaxDocumentBytes) return failure("Folder-purpose metadata exceeds the size limit");
    const QByteArray bytes(document.data(), static_cast<qsizetype>(document.size()));
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded = decoder(bytes);
    if (decoder.hasError()) return failure("Folder-purpose metadata is not valid UTF-8");
    const auto json = QJsonDocument::fromJson(decoded.toUtf8());
    if (!json.isObject()) return failure("Folder-purpose metadata must be a JSON object");
    const auto object = json.object();
    if (object.value("version") != QJsonValue(kMetadataVersion) || !object.value("folders").isObject()) {
        return failure("Folder-purpose metadata requires version:1 and a folders object");
    }
    const auto folders = object.value("folders").toObject();
    if (folders.size() > kMaxFolderDescriptions) return failure("Too many folder-purpose descriptions");
    Result result;
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        const auto path = it.key().toStdString();
        const auto text = parse_description(path, it.value());
        if (!text) return failure("Folder-purpose entry has an unsafe/nonliteral path or an invalid, empty or oversized description");
        result.purposes.emplace(path, *text);
    }
    return result;
}

FolderPurposeMetadata::Result FolderPurposeMetadata::load(const std::filesystem::path& root)
{
    const auto path = root / kFileName;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) return {};
    if (error || !std::filesystem::is_regular_file(status)) {
        return failure("Folder-purpose metadata must be a readable regular file, not a symlink");
    }
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) return failure("Cannot open folder-purpose metadata");
    if (file.size() > static_cast<qint64>(kMaxDocumentBytes)) return failure("Folder-purpose metadata exceeds the size limit");
    const auto bytes = file.read(static_cast<qint64>(kMaxDocumentBytes + 1));
    if (file.error() != QFileDevice::NoError) return failure("Cannot read folder-purpose metadata");
    return parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
}
