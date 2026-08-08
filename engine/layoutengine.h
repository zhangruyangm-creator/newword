#ifndef ENGINE_LAYOUTENGINE_H
#define ENGINE_LAYOUTENGINE_H

#include "documentmodel.h"
#include "pagelist.h"

namespace Engine {

class LayoutEngine
{
public:
    //! Layout in page-point coordinates (same units as PageDocumentPainter).
    [[nodiscard]] static LayoutResult layout(const DocumentModel &model,
                                             const PageLayoutSettings &fallbackSetup);

    [[nodiscard]] static int pageCount(const DocumentModel &model,
                                       const PageLayoutSettings &fallbackSetup);
};

} // namespace Engine

#endif // ENGINE_LAYOUTENGINE_H
