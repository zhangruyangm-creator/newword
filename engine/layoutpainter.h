#ifndef ENGINE_LAYOUTPAINTER_H
#define ENGINE_LAYOUTPAINTER_H

#include "pagelist.h"
#include "headerfootersettings.h"
#include "pagelayout.h"

class QPainter;

namespace Engine {

/** Paint one laid-out page in page-point coordinates (origin = page top-left). */
void paintLayoutPage(QPainter *painter,
                     const LayoutPage &page,
                     const PageLayoutSettings &layout,
                     const HeaderFooterSettings &headerFooter,
                     int totalPages,
                     qreal contentWidthPt,
                     qreal contentHeightPt);

} // namespace Engine

#endif // ENGINE_LAYOUTPAINTER_H
