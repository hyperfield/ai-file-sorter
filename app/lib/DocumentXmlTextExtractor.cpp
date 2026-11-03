#include "DocumentXmlTextExtractor.hpp"
#include "ContentAnalysisUnavailable.hpp"
#include "Utils.hpp"

#include <QElapsedTimer>
#include <QIODevice>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <algorithm>
#include <memory>
#include <stdexcept>

#if defined(AI_FILE_SORTER_USE_LIBZIP)
#include <zip.h>
#endif

namespace {
constexpr qint64 kMaxScannedXmlBytes = 64 * 1024 * 1024;
constexpr int kMaximumXmlNestingDepth = 256;
constexpr int kMaximumEntityExpansion = 4096;
#if !defined(AI_FILE_SORTER_USE_LIBZIP)
constexpr int kUnzipTimeoutMs = 7000;
constexpr int kUnzipCleanupTimeoutMs = 1000;
#endif

class XmlDevice final : public QIODevice {
public:
    XmlDevice(std::function<qint64(char*, qint64)> read, const std::function<bool()>& stop)
        : read_(std::move(read)), stop_(stop) { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
    bool empty() const { return read_bytes_ == 0; }
    void check() const {
        if (cancelled_) throw std::runtime_error("Document analysis cancelled.");
        if (limited_) throw std::runtime_error("Document XML exceeds the bounded scan limit before enough text can be extracted.");
        if (failed_) throw std::runtime_error("Could not read document XML.");
    }
protected:
    qint64 readData(char* data, qint64 size) override {
        if (stop_ && stop_()) { cancelled_ = true; return -1; }
        if (read_bytes_ >= kMaxScannedXmlBytes) { limited_ = true; return -1; }
        const auto count = read_(data, std::min(size, kMaxScannedXmlBytes - read_bytes_));
        if (count < 0) failed_ = true;
        else read_bytes_ += count;
        return count;
    }
    qint64 writeData(const char*, qint64) override { return -1; }
private:
    std::function<qint64(char*, qint64)> read_;
    std::function<bool()> stop_;
    qint64 read_bytes_{0};
    bool cancelled_{false}, limited_{false}, failed_{false};
};

/** @brief Track content elements independently of XML I/O and scan limits. */
class XmlTextCollector {
public:
    explicit XmlTextCollector(std::size_t limit) : limit_(limit) {}
    const std::string& text() const { return output_; }
    bool consume(const QXmlStreamReader& xml) {
        if (xml.isStartElement()) start_element(xml);
        else if (xml.isEndElement()) end_element();
        else if (xml.isCharacters() && text_depth_ && !xml.isWhitespace()) return append(xml.text());
        return false;
    }
private:
    bool captures(QStringView name) const {
        if (odf_) return body_depth_ && (name == QLatin1String("p") || name == QLatin1String("h"));
        return name == QLatin1String("t") || name == QLatin1String("v");
    }
    void start_element(const QXmlStreamReader& xml) {
        if (++depth_ > kMaximumXmlNestingDepth) throw ContentAnalysisUnavailable("Document XML nesting is too deep.");
        if (depth_ == 1) odf_ = xml.namespaceUri() == QLatin1String("urn:oasis:names:tc:opendocument:xmlns:office:1.0");
        if (odf_ && xml.name() == QLatin1String("body")) body_depth_ = depth_;
        if (!text_depth_ && captures(xml.name())) text_depth_ = depth_;
    }
    void end_element() {
        if (text_depth_ == depth_) text_depth_ = 0;
        if (body_depth_ == depth_) body_depth_ = 0;
        --depth_;
    }
    bool append(QStringView text) {
        const auto bytes = text.toString().simplified().toUtf8();
        if (bytes.isEmpty()) return false;
        if (!output_.empty() && output_.size() < limit_) output_.push_back(' ');
        auto count = std::min<std::size_t>(limit_ - output_.size(), bytes.size());
        while (count < static_cast<std::size_t>(bytes.size()) && count > 0 &&
               (static_cast<unsigned char>(bytes[count]) & 0xc0) == 0x80) --count;
        output_.append(bytes.constData(), count);
        return output_.size() >= limit_ || count < static_cast<std::size_t>(bytes.size());
    }
    const std::size_t limit_;
    std::string output_;
    int depth_{0}, body_depth_{0}, text_depth_{0};
    bool odf_{false};
};

std::string read_text(XmlDevice& device, std::size_t limit, const std::function<bool()>& stop)
{
    QXmlStreamReader xml(&device);
    xml.setEntityExpansionLimit(kMaximumEntityExpansion);
    XmlTextCollector collector(limit);
    while (!xml.atEnd()) {
        if (stop && stop()) throw std::runtime_error("Document analysis cancelled.");
        xml.readNext();
        if (collector.consume(xml)) return collector.text();
    }
    device.check();
    if (device.empty()) return {};
    if (xml.hasError()) throw ContentAnalysisUnavailable("Invalid document XML: " + xml.errorString().toStdString());
    return collector.text();
}

#if !defined(AI_FILE_SORTER_USE_LIBZIP)
qint64 read_unzip(QProcess& process, const QElapsedTimer& elapsed, char* bytes, qint64 count)
{
    while (!process.bytesAvailable() && process.state() != QProcess::NotRunning) {
        const auto remaining = kUnzipTimeoutMs - elapsed.elapsed();
        if (remaining <= 0 || !process.waitForReadyRead(static_cast<int>(remaining))) {
            if (process.state() == QProcess::NotRunning) break;
            return -1;
        }
    }
    // Missing archive members close the read channel without any stdout.
    if (!process.bytesAvailable() && process.state() == QProcess::NotRunning) return 0;
    return process.read(bytes, count);
}

struct UnzipCleanup {
    QProcess& process;
    ~UnzipCleanup() {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(kUnzipCleanupTimeoutMs);
        }
    }
};

std::string extract_unzip_member(const QString& unzip, const std::filesystem::path& archive,
    const char* member, std::size_t limit, const std::function<bool()>& stop)
{
    QProcess process;
    process.start(unzip, {QStringLiteral("-p"), QString::fromStdString(Utils::path_to_utf8(archive)), QString::fromUtf8(member)});
    UnzipCleanup cleanup{process};
    if (!process.waitForStarted(kUnzipTimeoutMs)) return {};
    QElapsedTimer elapsed;
    elapsed.start();
    XmlDevice device([&](char* bytes, qint64 count) { return read_unzip(process, elapsed, bytes, count); }, stop);
    return read_text(device, limit, stop);
}
#endif
} // namespace

std::string DocumentXmlTextExtractor::extract(const std::filesystem::path& archive,
    std::initializer_list<const char*> members, std::size_t max_text_bytes,
    const std::function<bool()>& should_stop)
{
    if (!max_text_bytes) return {};
#if defined(AI_FILE_SORTER_USE_LIBZIP)
    std::unique_ptr<zip_t, decltype(&zip_close)> zip(
        zip_open(Utils::path_to_utf8(archive).c_str(), ZIP_RDONLY, nullptr), zip_close);
    if (!zip) return {};
    for (const auto* member : members) {
        std::unique_ptr<zip_file_t, decltype(&zip_fclose)> file(zip_fopen(zip.get(), member, 0), zip_fclose);
        if (!file) continue;
        XmlDevice device([&](char* bytes, qint64 count) { return zip_fread(file.get(), bytes, count); }, should_stop);
        auto text = read_text(device, max_text_bytes, should_stop);
        if (!text.empty()) return text;
    }
#else
    const auto unzip = QStandardPaths::findExecutable(QStringLiteral("unzip"));
    if (unzip.isEmpty()) return {};
    for (const auto* member : members) {
        auto text = extract_unzip_member(unzip, archive, member, max_text_bytes, should_stop);
        if (!text.empty()) return text;
    }
#endif
    return {};
}
