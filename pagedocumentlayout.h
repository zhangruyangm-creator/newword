#ifndef PAGEDOCUMENTLAYOUT_H
#define PAGEDOCUMENTLAYOUT_H

#include <QVector>

class QTextDocument;

struct PageRange {
    qreal start = 0;
    qreal end = 0;
};

//! Pagination model for the self-drawn paged editor.
//!
//! Qt lays the last overflowing line of a page exactly at the page height, so
//! pages are not on a uniform grid. This model paginates with a reduced layout
//! page height and computes each page's real doc-y range from block rects.
//! Edits rebuild only the affected page(s) on demand (incremental); a debounced
//! full pass (driven by the widget) keeps page count exact.
class PagedDocumentLayout
{
public:
    explicit PagedDocumentLayout(QTextDocument *document = nullptr);

    void setDocument(QTextDocument *document) { m_document = document; }
    void setMetrics(qreal contentWidth, qreal contentHeight);

    //! true = continuous strip (draft/web); false = real paper pages.
    void setContinuous(bool continuous) { m_continuous = continuous; }
    [[nodiscard]] bool continuous() const { return m_continuous; }

    void relayout();
    void recompute();       //!< full pass (also refreshes max line height)
    void applyContinuous(); //!< single strip layout
    void markDirty(int position);
    void ensureRangesThroughPage(int pageIndex) const;

    [[nodiscard]] int pageIndexForDocY(qreal docY) const;
    [[nodiscard]] int pageCount() const { return m_pageCount; }
    [[nodiscard]] qreal layoutPageHeight() const { return m_layoutPageHeight; }
    [[nodiscard]] qreal maxLineHeight() const { return m_maxLineHeight; }
    [[nodiscard]] const QVector<PageRange> &ranges() const { return m_pageRanges; }
    [[nodiscard]] bool recomputing() const { return m_recomputing; }
    [[nodiscard]] bool isDirty() const
    {
        return m_maxLineHeightDirty || m_rebuildBlock >= 0;
    }
    [[nodiscard]] bool maxLineHeightDirty() const { return m_maxLineHeightDirty; }
    void setMaxLineHeightDirty(bool dirty) { m_maxLineHeightDirty = dirty; }

private:
    void ensurePageRangeSize(int pageIndex) const;

    QTextDocument *m_document = nullptr;
    qreal m_contentWidth = 0;
    qreal m_contentHeight = 0;
    qreal m_layoutPageHeight = 0;
    qreal m_maxLineHeight = 1;
    bool m_continuous = false;
    bool m_maxLineHeightDirty = true;
    mutable bool m_recomputing = false;
    mutable int m_rebuildBlock = -1; //!< first stale block; -1 = ranges fully accurate
    mutable int m_rangeBuiltToPage = -1;
    mutable int m_pageCount = 1;
    mutable QVector<PageRange> m_pageRanges;
    mutable QVector<int> m_pageStartBlocks;
};

#endif // PAGEDOCUMENTLAYOUT_H
