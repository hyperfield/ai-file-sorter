#include "ImageDecodeUtils.hpp"

#include "Logger.hpp"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QIODevice>
#include <QLibrary>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr const char* kPngImageFormat = "PNG";

bool is_webp_path(const QString& image_path)
{
    return QFileInfo(image_path).suffix().compare(QStringLiteral("webp"), Qt::CaseInsensitive) == 0;
}

QSize scaled_decode_size(const QSize& original_size, const QSize& max_size)
{
    if (!original_size.isValid() || !max_size.isValid()) {
        return {};
    }

    return original_size.scaled(max_size, Qt::KeepAspectRatio);
}

QImage scale_decoded_image(const QImage& image, const QSize& max_size)
{
    if (image.isNull() || !max_size.isValid()) {
        return image;
    }
    if (image.width() <= max_size.width() && image.height() <= max_size.height()) {
        return image;
    }

    return image.scaled(max_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

class WebpDecoderLibrary {
public:
    using GetInfoFn = int (*)(const std::uint8_t*, size_t, int*, int*);
    using DecodeRgbaFn = std::uint8_t* (*)(const std::uint8_t*, size_t, int*, int*);
    using FreeFn = void (*)(void*);

    WebpDecoderLibrary()
    {
        std::string last_error;
        for (const auto& candidate : library_candidates()) {
            if (candidate.version > 0) {
                library_.setFileNameAndVersion(QString::fromLatin1(candidate.name),
                                               candidate.version);
            } else {
                library_.setFileName(QString::fromLatin1(candidate.name));
            }
            if (!library_.load()) {
                last_error = library_.errorString().toStdString();
                continue;
            }

            get_info_ = reinterpret_cast<GetInfoFn>(library_.resolve("WebPGetInfo"));
            decode_rgba_ = reinterpret_cast<DecodeRgbaFn>(library_.resolve("WebPDecodeRGBA"));
            free_ = reinterpret_cast<FreeFn>(library_.resolve("WebPFree"));
            if (available()) {
                return;
            }

            error_ = "libwebp loaded but required decoder symbols were missing.";
            library_.unload();
            get_info_ = nullptr;
            decode_rgba_ = nullptr;
            free_ = nullptr;
        }

        if (error_.empty()) {
            error_ = last_error.empty() ? "libwebp was not found." : last_error;
        }
    }

    bool available() const
    {
        return get_info_ && decode_rgba_ && free_;
    }

    QImage decode_rgba(const QByteArray& bytes, std::string* error) const
    {
        if (!available()) {
            if (error) {
                *error = error_;
            }
            return {};
        }
        if (bytes.isEmpty()) {
            if (error) {
                *error = "WebP file is empty.";
            }
            return {};
        }

        const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.constData());
        const size_t size = static_cast<size_t>(bytes.size());
        int width = 0;
        int height = 0;
        if (get_info_(data, size, &width, &height) == 0 || width <= 0 || height <= 0) {
            if (error) {
                *error = "libwebp rejected the WebP header.";
            }
            return {};
        }

        std::uint8_t* decoded = decode_rgba_(data, size, &width, &height);
        if (!decoded || width <= 0 || height <= 0) {
            if (decoded) {
                free_(decoded);
            }
            if (error) {
                *error = "libwebp could not decode the image.";
            }
            return {};
        }

        const int bytes_per_line = width * 4;
        QImage view(decoded, width, height, bytes_per_line, QImage::Format_RGBA8888);
        QImage copy = view.copy();
        free_(decoded);
        if (copy.isNull() && error) {
            *error = "Decoded WebP pixels could not be copied into a QImage.";
        }
        return copy;
    }

private:
    struct LibraryCandidate {
        const char* name;
        int version;
    };

    static const std::array<LibraryCandidate, 13>& library_candidates()
    {
        static const std::array<LibraryCandidate, 13> candidates = {{
            {"webp", 7},
            {"webp", 0},
            {"libwebp", 0},
            {"libwebp.so.7", 0},
            {"libwebp.so", 0},
            {"/lib/x86_64-linux-gnu/libwebp.so.7", 0},
            {"/usr/lib/x86_64-linux-gnu/libwebp.so.7", 0},
            {"/lib/aarch64-linux-gnu/libwebp.so.7", 0},
            {"/usr/lib/aarch64-linux-gnu/libwebp.so.7", 0},
            {"libwebp.7.dylib", 0},
            {"libwebp.dylib", 0},
            {"libwebp.dll", 0},
            {"libwebp-7.dll", 0}
        }};
        return candidates;
    }

    QLibrary library_;
    GetInfoFn get_info_{nullptr};
    DecodeRgbaFn decode_rgba_{nullptr};
    FreeFn free_{nullptr};
    std::string error_;
};

QByteArray read_image_file_bytes(const QString& image_path, std::string* error)
{
    QFile file(image_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return {};
    }
    return file.readAll();
}

QImage decode_webp_with_libwebp(const QString& image_path,
                                const QSize& max_size,
                                std::string* error)
{
    std::string read_error;
    const QByteArray bytes = read_image_file_bytes(image_path, &read_error);
    if (bytes.isEmpty()) {
        if (error) {
            *error = read_error.empty() ? "WebP file is empty." : read_error;
        }
        return {};
    }

    static const WebpDecoderLibrary decoder;
    return scale_decoded_image(decoder.decode_rgba(bytes, error), max_size);
}

} // namespace

namespace ImageDecodeUtils {

QImage decode_image_with_webp_fallback(const QString& image_path,
                                       const QSize& max_size,
                                       std::string* error)
{
    QImageReader reader(image_path);
    reader.setAutoTransform(true);

    const QSize requested_size = scaled_decode_size(reader.size(), max_size);
    if (requested_size.isValid()) {
        reader.setScaledSize(requested_size);
    }

    const QImage qt_image = reader.read();
    if (!qt_image.isNull()) {
        return qt_image;
    }

    const std::string qt_error = reader.errorString().toUtf8().toStdString();
    if (!is_webp_path(image_path)) {
        if (error) {
            *error = qt_error.empty() ? "Qt image reader could not decode the image." : qt_error;
        }
        return {};
    }

    std::string libwebp_error;
    QImage libwebp_image = decode_webp_with_libwebp(image_path, max_size, &libwebp_error);
    if (!libwebp_image.isNull()) {
        if (auto logger = Logger::get_logger("core_logger")) {
            const std::string image_path_utf8 = image_path.toUtf8().toStdString();
            logger->info("Decoded WebP image '{}' with libwebp fallback after Qt reader failed: {}",
                         image_path_utf8,
                         qt_error.empty() ? "unknown Qt image reader error" : qt_error);
        }
        return libwebp_image;
    }

    if (error) {
        std::string message = "Failed to decode WebP image";
        if (!qt_error.empty()) {
            message += " (Qt: " + qt_error + ")";
        }
        if (!libwebp_error.empty()) {
            message += " (libwebp: " + libwebp_error + ")";
        }
        *error = message;
    }
    return {};
}

bool encode_image_as_png_bytes(const QImage& image, QByteArray& png_bytes)
{
    QBuffer buffer(&png_bytes);
    return !image.isNull() &&
           buffer.open(QIODevice::WriteOnly) &&
           image.save(&buffer, kPngImageFormat);
}

} // namespace ImageDecodeUtils
