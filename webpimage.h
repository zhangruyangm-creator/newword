#ifndef WEBPIMAGE_H
#define WEBPIMAGE_H

#include <QImage>
#include <QString>

//! Decode a WebP file via macOS ImageIO (Qt's bundled image formats lack WebP).
QImage loadWebpImage(const QString &filePath);

#endif // WEBPIMAGE_H
