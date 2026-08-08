#ifndef PAGEGEOMETRY_H
#define PAGEGEOMETRY_H

#include "pagelayout.h"

#include <QtGlobal>

//! Pure page metrics in pixels. No widgets — safe to unit-test and reuse.
struct PageGeometry
{
    int pageWidthPx = 0;
    int pageHeightPx = 0;
    int marginPx = 0;
    qreal zoomFactor = 1.0;

    [[nodiscard]] int bodyHeightPx(int headerH, int footerH) const;
    [[nodiscard]] int chromeLabelHeightPx(bool visible) const;

    //! Content-box height in px (margins + header/footer distance), matching LayoutEngine.
    [[nodiscard]] static int contentBodyHeightPx(const PageLayoutSettings &layout, int zoomPercent);

    [[nodiscard]] static qreal mmToPx(qreal mm);
    [[nodiscard]] static qreal zoomFactorFor(int zoomPercent);
    [[nodiscard]] static PageGeometry from(const PageLayoutSettings &layout, int zoomPercent);
};

#endif // PAGEGEOMETRY_H
