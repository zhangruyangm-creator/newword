#include "webpimage.h"

#include <webp/decode.h>

#include <QFile>

#include <cstring>

//! Decode a WebP file via libwebp (Qt's bundled image formats lack WebP).
QImage loadWebpImage(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray data = file.readAll();
    const auto *bytes = reinterpret_cast<const uint8_t *>(data.constData());
    const size_t size = static_cast<size_t>(data.size());

    int width = 0;
    int height = 0;
    if (!WebPGetInfo(bytes, size, &width, &height) || width <= 0 || height <= 0)
        return {};

    // WebPDecodeRGBA returns non-premultiplied R,G,B,A bytes — matches
    // QImage::Format_RGBA8888 byte order.
    uint8_t *rgba = WebPDecodeRGBA(bytes, size, &width, &height);
    if (!rgba)
        return {};

    QImage image(width, height, QImage::Format_RGBA8888);
    if (!image.isNull()) {
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        for (int y = 0; y < height; ++y)
            std::memcpy(image.scanLine(y), rgba + static_cast<size_t>(y) * rowBytes, rowBytes);
        image = image.convertToFormat(QImage::Format_ARGB32);
    }
    WebPFree(rgba);
    return image;
}
