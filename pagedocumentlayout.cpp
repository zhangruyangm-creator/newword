#include "pagedocumentlayout.h"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextLine>

#include <algorithm>
#include <cmath>
#include <limits>

PagedDocumentLayout::PagedDocumentLayout(QTextDocument *document)
    : m_document(document)
{
}

void PagedDocumentLayout::setMetrics(qreal contentWidth, qreal contentHeight)
{
    m_contentWidth = contentWidth;
    m_contentHeight = contentHeight;
}

void PagedDocumentLayout::relayout()
{
    if (m_continuous)
        applyContinuous();
    else
        recompute();
}

void PagedDocumentLayout::recompute()
{
    if (!m_document || m_recomputing)
        return;
    m_recomputing = true;

    auto *layout = m_document->documentLayout();
    (void)layout->documentSize(); // force full layout so all line heights are valid

    qreal maxLineHeight = 1.0;
    for (QTextBlock b = m_document->begin(); b.isValid(); b = b.next()) {
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i)
            maxLineHeight = qMax(maxLineHeight, tl->lineAt(i).height());
    }
    m_maxLineHeight = maxLineHeight;

    // Qt places the line that does not fit at exactly the page height, then
    // starts the next page after it. Shrink the layout page height by the max
    // line height so that overflowing line still ends inside the content box.
    m_layoutPageHeight = qMax(120.0, m_contentHeight - maxLineHeight - 1.0);
    m_document->setPageSize(QSizeF(m_contentWidth, m_layoutPageHeight));
    (void)layout->documentSize();

    QVector<qreal> minTop;
    QVector<qreal> maxBottom;
    QVector<int> startBlocks;
    for (QTextBlock b = m_document->begin(); b.isValid(); b = b.next()) {
        const QRectF br = layout->blockBoundingRect(b);
        if (!br.isValid())
            continue;
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i) {
            const QTextLine ln = tl->lineAt(i);
            const qreal top = br.top() + ln.y();
            const int page = qMax(0, int(std::floor(top / m_layoutPageHeight)));
            while (minTop.size() <= page) {
                minTop.append(std::numeric_limits<qreal>::max());
                maxBottom.append(std::numeric_limits<qreal>::lowest());
            }
            minTop[page] = qMin(minTop[page], top);
            maxBottom[page] = qMax(maxBottom[page], top + ln.height());
            if (startBlocks.size() <= page)
                startBlocks.resize(page + 1, b.blockNumber());
        }
    }

    m_pageRanges.clear();
    if (minTop.isEmpty()) {
        m_pageRanges.append(PageRange{0.0, m_contentHeight});
    } else {
        m_pageRanges.reserve(minTop.size());
        for (int i = 0; i < minTop.size(); ++i)
            m_pageRanges.append(PageRange{minTop.at(i),
                                          qMax(minTop.at(i) + 1.0, maxBottom.at(i))});
    }
    m_pageCount = m_pageRanges.size();
    m_pageStartBlocks = startBlocks;
    m_rangeBuiltToPage = m_pageCount - 1;
    m_rebuildBlock = -1;
    m_maxLineHeightDirty = false;
    m_recomputing = false;
}

void PagedDocumentLayout::applyContinuous()
{
    if (!m_document)
        return;
    m_document->setPageSize(QSizeF(m_contentWidth, -1));
    (void)m_document->documentLayout()->documentSize();
    m_pageRanges.clear();
    m_pageRanges.append(PageRange{0.0, qMax(m_contentHeight, m_document->size().height())});
    m_pageCount = 1;
}

void PagedDocumentLayout::markDirty(int position)
{
    if (!m_document)
        return;
    const QTextBlock b = m_document->findBlock(position);
    const int bn = b.isValid() ? b.blockNumber() : 0;
    m_rebuildBlock = (m_rebuildBlock < 0) ? bn : qMin(m_rebuildBlock, bn);

    // Pages before the block's page are untouched; everything from that page on
    // must be re-walked. pageStartBlocks is still valid up to the old built page.
    int page = 0;
    for (int i = 0; i < m_pageStartBlocks.size(); ++i) {
        const int sb = m_pageStartBlocks.at(i);
        if (sb >= 0 && sb <= bn)
            page = i;
        else
            break;
    }
    // The edit page's end is now unknown — reset it so the rebuild does not
    // keep a stale (possibly too large) end after deletions.
    if (page < m_pageRanges.size())
        m_pageRanges[page].end = m_pageRanges[page].start;
    m_rangeBuiltToPage = qMin(m_rangeBuiltToPage, page - 1);
}

void PagedDocumentLayout::ensurePageRangeSize(int pageIndex) const
{
    if (m_pageRanges.size() <= pageIndex)
        m_pageRanges.resize(pageIndex + 1, PageRange{0.0, 0.0});
}

void PagedDocumentLayout::ensureRangesThroughPage(int pageIndex) const
{
    if (!m_document || m_continuous)
        return;
    if (m_maxLineHeightDirty) {
        if (!m_recomputing)
            const_cast<PagedDocumentLayout *>(this)->recompute();
        return;
    }
    if (m_rebuildBlock < 0 || pageIndex <= m_rangeBuiltToPage)
        return;

    auto *layout = m_document->documentLayout();
    const int startPage = qMax(0, m_rangeBuiltToPage + 1);
    int blockNum = m_rebuildBlock;
    int page = startPage;
    qreal minTop = std::numeric_limits<qreal>::max();
    qreal maxBottom = std::numeric_limits<qreal>::lowest();
    const bool editInFirstBlock =
        blockNum <= (page < m_pageStartBlocks.size() ? m_pageStartBlocks.at(page) : -1);
    if (!editInFirstBlock && page < m_pageRanges.size())
        minTop = m_pageRanges.at(page).start;

    m_recomputing = true;
    auto finishCurrentPage = [&]() -> bool {
        if (minTop >= std::numeric_limits<qreal>::max()) {
            // The page turned out empty / start moved — safe full pass.
            m_recomputing = false;
            const_cast<PagedDocumentLayout *>(this)->recompute();
            return false;
        }
        ensurePageRangeSize(page);
        m_pageRanges[page].end = qMax(m_pageRanges[page].end, maxBottom);
        if (page == startPage) {
            if (editInFirstBlock)
                m_pageRanges[page].start = minTop;
            else
                m_pageRanges[page].start = qMin(m_pageRanges[page].start, minTop);
        }
        if (m_pageStartBlocks.size() <= page)
            m_pageStartBlocks.resize(page + 1, -1);
        if (page > startPage || m_pageStartBlocks.at(page) < 0)
            m_pageStartBlocks[page] = blockNum;
        return true;
    };

    while (blockNum < m_document->blockCount()) {
        const QTextBlock b = m_document->findBlockByNumber(blockNum);
        const QRectF br = layout->blockBoundingRect(b);
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i) {
            const QTextLine ln = tl->lineAt(i);
            const qreal top = br.top() + ln.y();
            const int lp = qMax(0, int(std::floor(top / m_layoutPageHeight)));
            const qreal bottom = top + ln.height();
            if (lp < page) {
                if (lp != page - 1) {
                    // Content moved onto a much earlier page (large delete) —
                    // safe path: full pass.
                    m_recomputing = false;
                    const_cast<PagedDocumentLayout *>(this)->recompute();
                    return;
                }
                // Boundary off-by-one: the first walked line sits on the page
                // before the expected start (a block straddles the break).
                // Back up one page and rebuild its end from the walk.
                page = lp;
                maxBottom = std::numeric_limits<qreal>::lowest();
                if (page < m_pageRanges.size())
                    m_pageRanges[page].end = m_pageRanges[page].start;
            }
            if (lp > page) {
                if (!finishCurrentPage())
                    return;
                if (lp > page + 1) {
                    // Empty pages in between — drop them.
                    m_pageRanges.resize(page + 1);
                    m_pageStartBlocks.resize(qMax(0, page));
                }
                page = lp;
                minTop = top;
                maxBottom = bottom;
                ensurePageRangeSize(page);
                m_pageRanges[page].start = top;
                if (page > pageIndex) {
                    if (m_pageStartBlocks.size() <= page)
                        m_pageStartBlocks.resize(page + 1, -1);
                    m_pageStartBlocks[page] = blockNum;
                    m_pageCount = qMax(m_pageCount, page + 1);
                    m_rangeBuiltToPage = pageIndex;
                    m_rebuildBlock = blockNum; // continue from this block next time
                    m_recomputing = false;
                    return;
                }
            } else {
                minTop = qMin(minTop, top);
                maxBottom = qMax(maxBottom, bottom);
            }
        }
        ++blockNum;
    }

    // Reached the end of the document: everything is accurate now.
    if (minTop >= std::numeric_limits<qreal>::max()) {
        m_recomputing = false;
        const_cast<PagedDocumentLayout *>(this)->recompute();
        return;
    }
    if (!finishCurrentPage())
        return;
    m_pageRanges.resize(page + 1);
    m_pageStartBlocks.resize(page + 1);
    m_pageCount = page + 1;
    m_rangeBuiltToPage = page;
    m_rebuildBlock = -1;
    m_recomputing = false;
}

int PagedDocumentLayout::pageIndexForDocY(qreal docY) const
{
    if (m_pageRanges.isEmpty())
        return 0;
    int idx = 0;
    for (int i = 0; i < m_pageRanges.size(); ++i) {
        if (docY >= m_pageRanges.at(i).start)
            idx = i;
        else
            break;
    }
    return qBound(0, idx, m_pageRanges.size() - 1);
}
