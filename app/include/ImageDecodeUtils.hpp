/**
 * @file ImageDecodeUtils.hpp
 * @brief Shared image decoding helpers for visual analysis.
 */
#pragma once

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

#include <string>

namespace ImageDecodeUtils {

/**
 * @brief Decode an image with Qt and fall back to runtime libwebp for WebP files.
 * @param image_path Absolute or relative path to the image file.
 * @param max_size Optional maximum size for the decoded image; invalid values keep original size.
 * @param error Receives a human-readable failure reason when decoding fails.
 * @return Decoded image, or a null image on failure.
 */
QImage decode_image_with_webp_fallback(const QString& image_path,
                                       const QSize& max_size = QSize(),
                                       std::string* error = nullptr);

/**
 * @brief Encode a decoded image as PNG bytes.
 * @param image Image to encode.
 * @param png_bytes Destination buffer for encoded PNG data.
 * @return True when encoding succeeds.
 */
bool encode_image_as_png_bytes(const QImage& image, QByteArray& png_bytes);

} // namespace ImageDecodeUtils
