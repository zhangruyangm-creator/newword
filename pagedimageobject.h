#ifndef PAGEDIMAGEOBJECT_H
#define PAGEDIMAGEOBJECT_H

#include <QCache>
#include <QObject>
#include <QTextFormat>
#include <QTextObjectInterface>

class QPainter;
class QPixmap;

//! Image object handler with a scaled-pixmap cache: each image is scaled once
//! per target size, then blitted — large-photo documents repaint cheaply.
class PagedImageObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    explicit PagedImageObject(QObject *parent = nullptr);

    QSizeF intrinsicSize(QTextDocument *document, int, const QTextFormat &format) override;
    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *document, int,
                    const QTextFormat &format) override;

private:
    QCache<QString, QPixmap> m_cache;
};

#endif // PAGEDIMAGEOBJECT_H
