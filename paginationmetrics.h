#ifndef PAGINATIONMETRICS_H
#define PAGINATIONMETRICS_H

#include "pagelayout.h"

#include <QString>
#include <QtGlobal>

class QTextDocument;

/** Thesis eval: geometric estimate vs LayoutEngine; live view uses engine for count. */
namespace PaginationMetrics {

struct CompareResult {
    QString sampleName;
    int geometricPages = 1; //!< QTextDocument height ÷ content body (legacy estimate)
    int enginePages = 1;    //!< LayoutEngine (preview/PDF + live pageCount)
    int livePages = 1;      //!< Same as enginePages after EditorViewLayout wiring

    [[nodiscard]] int geometricDelta() const { return enginePages - geometricPages; }
    [[nodiscard]] int liveDelta() const { return enginePages - livePages; }
    [[nodiscard]] bool geometricWithinTolerance(int tol = 1) const
    {
        return qAbs(geometricDelta()) <= tol;
    }
    [[nodiscard]] bool liveMatchesEngine() const { return liveDelta() == 0; }

    // Back-compat aliases used by older call sites / docs wording.
    [[nodiscard]] int visualPages() const { return geometricPages; }
    [[nodiscard]] int delta() const { return geometricDelta(); }
    [[nodiscard]] int absDelta() const { return qAbs(geometricDelta()); }
    [[nodiscard]] bool withinTolerance(int tol = 1) const
    {
        return geometricWithinTolerance(tol);
    }
};

[[nodiscard]] int geometricPageCount(QTextDocument *document,
                                     const PageLayoutSettings &layout,
                                     int zoomPercent = 100);

[[nodiscard]] int enginePageCount(QTextDocument *document,
                                  const PageLayoutSettings &layout);

/** Live page view count policy: LayoutEngine (matches PDF). */
[[nodiscard]] inline int livePageCount(QTextDocument *document,
                                       const PageLayoutSettings &layout)
{
    return enginePageCount(document, layout);
}

/** @deprecated Prefer geometricPageCount — kept for call-site clarity during transition. */
[[nodiscard]] inline int visualPageCount(QTextDocument *document,
                                         const PageLayoutSettings &layout,
                                         int zoomPercent = 100)
{
    return geometricPageCount(document, layout, zoomPercent);
}

[[nodiscard]] CompareResult compare(const QTextDocument *document,
                                    const PageLayoutSettings &layout,
                                    const QString &sampleName = {},
                                    int zoomPercent = 100);

[[nodiscard]] QString markdownTableRow(const CompareResult &r);
[[nodiscard]] QString markdownTableHeader();

} // namespace PaginationMetrics

#endif // PAGINATIONMETRICS_H
