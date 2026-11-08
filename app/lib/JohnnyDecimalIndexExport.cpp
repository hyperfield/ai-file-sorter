#include "JohnnyDecimalIndexExport.hpp"

#include <QSaveFile>

#include "Utils.hpp"

bool JohnnyDecimalIndexExport::save(const JohnnyDecimalArchiveIndex::Index& index,
                                    const std::filesystem::path& destination, std::string* error) {
    if (error) {
        error->clear();
    }
    QSaveFile file(QString::fromStdString(Utils::path_to_utf8(destination)));
    const auto bytes = QByteArray::fromStdString(JohnnyDecimalArchiveIndex::format_markdown(index));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    return true;
}
