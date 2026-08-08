#include "paginationmetrics.h"
#include "layoutengine.h"
#include "pagegeometry.h"
#include "qtextadapter.h"

#include <QAbstractTextDocumentLayout>
#include <QTextDocument>

#include <memory>

namespace PaginationMetrics {

int geometricPageCount(QTextDocument *document,
                       const PageLayoutSettings &layout,
                       int zoomPercent)
{
    if (!document)
        return 1;

    const PageGeometry geo = PageGeometry::from(layout, zoomPercent);
    document->setDocumentMargin(geo.marginPx);
    document->setPageSize(QSizeF(geo.pageWidthPx, -1));
    if (auto *dl = document->documentLayout())
        (void)dl->documentSize();

    const int onePageBodyPx = PageGeometry::contentBodyHeightPx(layout, zoomPercent);
    const int docHeight = qMax(1, qRound(document->size().height()) + 24);
    const int editorHeightPx = qMax(onePageBodyPx, docHeight);
    return qMax(1, (editorHeightPx + onePageBodyPx - 1) / onePageBodyPx);
}

int enginePageCount(QTextDocument *document, const PageLayoutSettings &layout)
{
    if (!document)
        return 1;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(document, layout);
    return Engine::LayoutEngine::pageCount(model, layout);
}

CompareResult compare(const QTextDocument *document,
                      const PageLayoutSettings &layout,
                      const QString &sampleName,
                      int zoomPercent)
{
    CompareResult r;
    r.sampleName = sampleName;
    if (!document)
        return r;

    std::unique_ptr<QTextDocument> geometricClone(document->clone());
    std::unique_ptr<QTextDocument> engineClone(document->clone());
    r.geometricPages = geometricPageCount(geometricClone.get(), layout, zoomPercent);
    r.enginePages = enginePageCount(engineClone.get(), layout);
    r.livePages = r.enginePages; // EditorViewLayout uses LayoutEngine for pageCount
    return r;
}

QString markdownTableHeader()
{
    return QStringLiteral(
        "| 样例 | 几何估算 | 活页(引擎) | LayoutEngine | Δ几何 | Δ活页 |\n"
        "|------|--------:|----------:|-------------:|------:|------:|\n");
}

QString markdownTableRow(const CompareResult &r)
{
    const QString gDelta = r.geometricDelta() > 0
        ? QStringLiteral("+%1").arg(r.geometricDelta())
        : QString::number(r.geometricDelta());
    const QString lDelta = r.liveDelta() > 0
        ? QStringLiteral("+%1").arg(r.liveDelta())
        : QString::number(r.liveDelta());
    return QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 |\n")
        .arg(r.sampleName.isEmpty() ? QStringLiteral("(unnamed)") : r.sampleName)
        .arg(r.geometricPages)
        .arg(r.livePages)
        .arg(r.enginePages)
        .arg(gDelta)
        .arg(lDelta);
}

} // namespace PaginationMetrics
