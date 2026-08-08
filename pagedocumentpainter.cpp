#include "pagedocumentpainter.h"
#include "documentsections.h"
#include "layoutengine.h"
#include "layoutpainter.h"
#include "qtextadapter.h"

#include <QPainter>
#include <QPrinter>
#include <QTextDocument>

#include <memory>

namespace PageDocumentPainter {

Engine::LayoutResult layoutDocument(QTextDocument *document, const PageLayoutSettings &layout)
{
    return Engine::LayoutEngine::layout(
        Engine::QTextAdapter::fromDocument(document, layout), layout);
}

qreal mmToPoints(qreal mm)
{
    return mm * 2.834645669;
}

QSizeF pageSizePoints(const PageLayoutSettings &layout)
{
    const QSizeF mm = layout.pageSizeMm();
    return {mmToPoints(mm.width()), mmToPoints(mm.height())};
}

QRectF contentRectPoints(const PageLayoutSettings &layout)
{
    const QSizeF page = pageSizePoints(layout);
    const qreal left = mmToPoints(layout.marginsMm.left());
    const qreal right = mmToPoints(layout.marginsMm.right());
    const qreal top = mmToPoints(layout.marginsMm.top()) + mmToPoints(layout.headerDistanceMm);
    const qreal bottom = mmToPoints(layout.marginsMm.bottom()) + mmToPoints(layout.footerDistanceMm);
    return QRectF(left, top, page.width() - left - right, page.height() - top - bottom);
}

int pageCount(QTextDocument *document, const PageLayoutSettings &layout)
{
    if (!document)
        return 1;
    return layoutDocument(document, layout).pageCount();
}

void paintLaidOutPage(QPainter *painter,
                      int pageIndex,
                      const Engine::LayoutResult &result,
                      const PageLayoutSettings &layout,
                      const HeaderFooterSettings &headerFooter,
                      int totalPages)
{
    if (!painter || pageIndex < 0 || pageIndex >= result.pages.size())
        return;

    Engine::paintLayoutPage(painter,
                            result.pages.at(pageIndex),
                            layout,
                            headerFooter,
                            totalPages > 0 ? totalPages : result.pageCount(),
                            result.contentWidthPt,
                            result.contentHeightPt);
}

void paintPage(QPainter *painter,
               int pageIndex,
               QTextDocument *document,
               const PageLayoutSettings &layout,
               const HeaderFooterSettings &headerFooter,
               int totalPages)
{
    if (!painter || !document)
        return;
    paintLaidOutPage(painter, pageIndex, layoutDocument(document, layout), layout,
                     headerFooter, totalPages);
}

bool printDocument(QPrinter *printer,
                   QTextDocument *source,
                   const PageLayoutSettings &layout,
                   const HeaderFooterSettings &headerFooter)
{
    if (!printer || !source)
        return false;

    std::unique_ptr<QTextDocument> doc(source->clone());
    const Engine::LayoutResult result = layoutDocument(doc.get(), layout);
    const int pages = result.pageCount();

    printer->setFullPage(true);
    printer->setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPainter painter(printer);
    if (!painter.isActive())
        return false;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    for (int i = 0; i < pages; ++i) {
        if (i > 0)
            printer->newPage();

        const auto sections = DocumentSections::sectionsFromDocument(doc.get(), layout, headerFooter);
        const DocumentSections::Section section =
            DocumentSections::sectionForPage(sections, doc.get(), layout, i);

        const QRectF devicePage = printer->pageRect(QPrinter::DevicePixel);
        const QSizeF sectionPagePts = pageSizePoints(section.layout);
        const qreal scale = qMin(devicePage.width() / sectionPagePts.width(),
                                 devicePage.height() / sectionPagePts.height());
        painter.save();
        painter.translate(devicePage.topLeft());
        painter.scale(scale, scale);
        paintLaidOutPage(&painter, i, result, section.layout, section.headerFooter, pages);
        painter.restore();
    }
    return true;
}

} // namespace PageDocumentPainter
