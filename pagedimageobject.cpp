#include "pagedimageobject.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QVariant>

PagedImageObject::PagedImageObject(QObject *parent)
    : QObject(parent)
    , m_cache(2048) // ~128 MB of scaled pixmaps
{
}

QSizeF PagedImageObject::intrinsicSize(QTextDocument *document, int,
                                       const QTextFormat &format)
{
    const QTextImageFormat imageFormat = format.toImageFormat();
    const QSizeF fmtSize(imageFormat.width(), imageFormat.height());
    const QImage image = qvariant_cast<QImage>(
        document->resource(QTextDocument::ImageResource, imageFormat.name()));
    if (fmtSize.width() > 0 && fmtSize.height() > 0)
        return fmtSize;
    if (image.isNull())
        return fmtSize;
    QSizeF size = image.size();
    if (fmtSize.width() > 0) {
        size.setHeight(size.height() * fmtSize.width() / size.width());
        size.setWidth(fmtSize.width());
    } else if (fmtSize.height() > 0) {
        size.setWidth(size.width() * fmtSize.height() / size.height());
        size.setHeight(fmtSize.height());
    }
    return size;
}

void PagedImageObject::drawObject(QPainter *painter, const QRectF &rect,
                                  QTextDocument *document, int, const QTextFormat &format)
{
    const QTextImageFormat imageFormat = format.toImageFormat();
    const QString name = imageFormat.name();
    if (name.isEmpty())
        return;
    const QImage image = qvariant_cast<QImage>(
        document->resource(QTextDocument::ImageResource, name));
    if (image.isNull())
        return;
    const QSize target = rect.size().toSize();
    if (target.width() <= 0 || target.height() <= 0)
        return;
    const QString key = QStringLiteral("%1@%2x%3").arg(name).arg(target.width())
                            .arg(target.height());
    QPixmap *cached = m_cache.object(key);
    if (!cached) {
        QPixmap pm = QPixmap::fromImage(image).scaled(
            target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (pm.isNull())
            return;
        m_cache.insert(key, new QPixmap(pm),
                       qMax(1, (pm.width() * pm.height() * 4) / 65536));
        cached = m_cache.object(key);
    }
    if (!cached)
        return;
    const QPointF offset((rect.width() - cached->width()) / 2.0,
                         (rect.height() - cached->height()) / 2.0);
    painter->drawPixmap(rect.topLeft() + offset, *cached);
}
