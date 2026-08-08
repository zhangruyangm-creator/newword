#include "imageprops.h"

#include <QImage>

namespace ImageProps {

Wrap wrapOf(const QTextImageFormat &fmt)
{
    if (!fmt.hasProperty(kWrapProperty))
        return Wrap::Block; // prefer block so live page ≈ engine
    return Wrap(fmt.property(kWrapProperty).toInt());
}

void setWrap(QTextImageFormat *fmt, Wrap wrap)
{
    if (!fmt)
        return;
    fmt->setProperty(kWrapProperty, int(wrap));
}

Qt::Alignment alignOf(const QTextImageFormat &fmt)
{
    if (!fmt.hasProperty(kAlignProperty))
        return Qt::AlignHCenter;
    return Qt::Alignment(fmt.property(kAlignProperty).toInt());
}

void setAlign(QTextImageFormat *fmt, Qt::Alignment align)
{
    if (!fmt)
        return;
    fmt->setProperty(kAlignProperty, int(align & Qt::AlignHorizontal_Mask));
}

void fitToMaxWidth(QTextImageFormat *fmt, const QImage &image, int maxWidthPx)
{
    if (!fmt || image.isNull() || maxWidthPx <= 0)
        return;
    const int w = image.width();
    const int h = image.height();
    if (w <= maxWidthPx) {
        if (fmt->width() <= 0)
            fmt->setWidth(w);
        if (fmt->height() <= 0)
            fmt->setHeight(h);
        return;
    }
    fmt->setWidth(maxWidthPx);
    fmt->setHeight(qMax(1, int(h * (qreal(maxWidthPx) / w))));
}

} // namespace ImageProps
