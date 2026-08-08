#include "pagegeometry.h"

#include <QtMath>

namespace {
constexpr qreal kMmToPx = 96.0 / 25.4;
}

qreal PageGeometry::mmToPx(qreal mm)
{
    return mm * kMmToPx;
}

qreal PageGeometry::zoomFactorFor(int zoomPercent)
{
    return qBound(0.5, zoomPercent / 100.0, 2.0);
}

PageGeometry PageGeometry::from(const PageLayoutSettings &layout, int zoomPercent)
{
    PageGeometry g;
    g.zoomFactor = zoomFactorFor(zoomPercent);
    const QSizeF pageMm = layout.pageSizeMm();
    g.pageWidthPx = qMax(200, qRound(mmToPx(pageMm.width()) * g.zoomFactor));
    g.pageHeightPx = qMax(200, qRound(mmToPx(pageMm.height()) * g.zoomFactor));

    const qreal marginMm = qMin(qMin(layout.marginsMm.left(), layout.marginsMm.right()),
                                qMin(layout.marginsMm.top(), layout.marginsMm.bottom()));
    g.marginPx = qMax(8, qRound(mmToPx(marginMm) * g.zoomFactor));
    return g;
}

int PageGeometry::bodyHeightPx(int headerH, int footerH) const
{
    return qMax(120, pageHeightPx - headerH - footerH);
}

int PageGeometry::contentBodyHeightPx(const PageLayoutSettings &layout, int zoomPercent)
{
    const qreal zoom = zoomFactorFor(zoomPercent);
    const qreal contentMm = layout.pageSizeMm().height()
                            - layout.marginsMm.top() - layout.marginsMm.bottom()
                            - layout.headerDistanceMm - layout.footerDistanceMm;
    return qMax(120, qRound(mmToPx(contentMm) * zoom));
}

int PageGeometry::chromeLabelHeightPx(bool visible) const
{
    if (!visible)
        return 0;
    return qMax(18, qRound(18 * zoomFactor));
}
