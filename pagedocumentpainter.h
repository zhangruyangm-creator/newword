#ifndef PAGEDOCUMENTPAINTER_H
#define PAGEDOCUMENTPAINTER_H

#include "headerfootersettings.h"
#include "pagelayout.h"
#include "engine/pagelist.h"

#include <QRectF>
#include <QSizeF>

class QPainter;
class QPrinter;
class QTextDocument;

namespace PageDocumentPainter {

qreal mmToPoints(qreal mm);
QSizeF pageSizePoints(const PageLayoutSettings &layout);
QRectF contentRectPoints(const PageLayoutSettings &layout);

/** Layout via Engine (QTextDocument → DocumentModel → LayoutEngine). */
[[nodiscard]] Engine::LayoutResult layoutDocument(QTextDocument *document,
                                                  const PageLayoutSettings &layout);

int pageCount(QTextDocument *document, const PageLayoutSettings &layout);

/** Paint one page in page-point coordinates (origin = page top-left). */
void paintPage(QPainter *painter,
               int pageIndex,
               QTextDocument *document,
               const PageLayoutSettings &layout,
               const HeaderFooterSettings &headerFooter,
               int totalPages);

/** Paint using a precomputed layout result (avoids re-layout per page). */
void paintLaidOutPage(QPainter *painter,
                      int pageIndex,
                      const Engine::LayoutResult &result,
                      const PageLayoutSettings &layout,
                      const HeaderFooterSettings &headerFooter,
                      int totalPages);

/** Print / export PDF with headers and footers matching preview. */
[[nodiscard]] bool printDocument(QPrinter *printer,
                   QTextDocument *source,
                   const PageLayoutSettings &layout,
                   const HeaderFooterSettings &headerFooter);

} // namespace PageDocumentPainter

#endif // PAGEDOCUMENTPAINTER_H
