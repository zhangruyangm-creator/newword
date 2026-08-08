#include "layoutengine.h"

#include <QRectF>
#include <QSet>
#include <QTextCharFormat>
#include <QTextLayout>
#include <Qt>

#include <cmath>

namespace Engine {
namespace {

constexpr qreal kMmToPoints = 2.834645669;
constexpr qreal kFootnoteRuleGapAbove = 6.0;
constexpr qreal kFootnoteRuleGapBelow = 4.0;
constexpr qreal kFootnoteRuleThickness = 0.5;
constexpr qreal kFootnoteRuleBand =
    kFootnoteRuleGapAbove + kFootnoteRuleThickness + kFootnoteRuleGapBelow;

struct FootnoteMarker {
    int textStart = 0;
    int textLength = 0;
    QString id;
    int number = 0;
};

struct PendingFootnote {
    QString id;
    int number = 0;
    QString body;
};

struct LayoutSession {
    const DocumentModel *model = nullptr;
    qreal fullHeight = 0;
    qreal contentWidth = 0;
    qreal footnoteReserved = 0;
    QVector<PendingFootnote> pending;
    QSet<QString> placedDoc; //!< finalized onto a previous page

    struct Snapshot {
        qreal footnoteReserved = 0;
        QVector<PendingFootnote> pending;
    };

    [[nodiscard]] qreal usableHeight() const
    {
        return qMax(40.0, fullHeight - footnoteReserved);
    }

    [[nodiscard]] Snapshot snapshot() const
    {
        return Snapshot{footnoteReserved, pending};
    }

    void restore(const Snapshot &s)
    {
        footnoteReserved = s.footnoteReserved;
        pending = s.pending;
    }

    void clearPageFootnotes()
    {
        footnoteReserved = 0;
        pending.clear();
    }

    [[nodiscard]] qreal measureNoteHeight(const PendingFootnote &note) const
    {
        QFont font;
        font.setPointSizeF(9.0);
        const QString text = QStringLiteral("%1. %2").arg(note.number).arg(note.body);
        QTextLayout layout(text, font);
        layout.beginLayout();
        qreal total = 0;
        qreal y = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(contentWidth);
            line.setPosition(QPointF(0, y));
            total += line.height();
            y += line.height();
        }
        layout.endLayout();
        return qMax(11.0, total);
    }

    [[nodiscard]] bool alreadyPending(const QString &id) const
    {
        for (const PendingFootnote &p : pending) {
            if (p.id == id)
                return true;
        }
        return false;
    }

    bool claim(const QString &id, int number)
    {
        if (id.isEmpty() || placedDoc.contains(id) || alreadyPending(id) || !model)
            return false;
        const QString body = model->footnoteBodies.value(id);
        PendingFootnote note{id, number > 0 ? number : 1, body};
        qreal add = measureNoteHeight(note);
        if (pending.isEmpty())
            add += kFootnoteRuleBand;
        footnoteReserved += add;
        pending.append(note);
        return true;
    }

    void claimInRange(const QVector<FootnoteMarker> &markers, int textStart, int textLength)
    {
        const int textEnd = textStart + textLength;
        for (const FootnoteMarker &m : markers) {
            if (m.textStart + m.textLength <= textStart || m.textStart >= textEnd)
                continue;
            claim(m.id, m.number);
        }
    }

    void finalizePage(LayoutPage *page)
    {
        if (!page || pending.isEmpty()) {
            clearPageFootnotes();
            return;
        }

        page->hasFootnoteRule = true;
        qreal y = fullHeight - footnoteReserved;
        page->footnoteRuleY = y;
        y += kFootnoteRuleBand;

        QFont font;
        font.setPointSizeF(9.0);
        for (const PendingFootnote &note : pending) {
            placedDoc.insert(note.id);
            const QString text = QStringLiteral("%1. %2").arg(note.number).arg(note.body);
            QTextLayout layout(text, font);
            layout.beginLayout();
            while (true) {
                QTextLine line = layout.createLine();
                if (!line.isValid())
                    break;
                line.setLineWidth(contentWidth);
                const qreal h = qMax(1.0, line.height());
                LayoutLine out;
                out.text = text.mid(line.textStart(), line.textLength());
                out.baseFont = font;
                out.x = 0;
                out.y = y;
                out.width = line.naturalTextWidth();
                out.height = h;
                out.ascent = line.ascent();
                page->footnoteLines.append(out);
                y += h;
            }
            layout.endLayout();
        }
        clearPageFootnotes();
    }
};

QRectF contentRectPoints(const PageLayoutSettings &layout)
{
    const QSizeF mm = layout.pageSizeMm();
    const QSizeF page(mm.width() * kMmToPoints, mm.height() * kMmToPoints);
    const qreal left = layout.marginsMm.left() * kMmToPoints;
    const qreal right = layout.marginsMm.right() * kMmToPoints;
    const qreal top = (layout.marginsMm.top() + layout.headerDistanceMm) * kMmToPoints;
    const qreal bottom = (layout.marginsMm.bottom() + layout.footerDistanceMm) * kMmToPoints;
    return QRectF(left, top, page.width() - left - right, page.height() - top - bottom);
}

bool pageSetupEquals(const PageLayoutSettings &a, const PageLayoutSettings &b)
{
    const auto nearly = [](qreal x, qreal y) { return std::abs(x - y) < 0.05; };
    return a.paper == b.paper
        && a.orientation == b.orientation
        && nearly(a.customWidthMm, b.customWidthMm)
        && nearly(a.customHeightMm, b.customHeightMm)
        && nearly(a.marginsMm.left(), b.marginsMm.left())
        && nearly(a.marginsMm.top(), b.marginsMm.top())
        && nearly(a.marginsMm.right(), b.marginsMm.right())
        && nearly(a.marginsMm.bottom(), b.marginsMm.bottom())
        && nearly(a.headerDistanceMm, b.headerDistanceMm)
        && nearly(a.footerDistanceMm, b.footerDistanceMm);
}

void flushPage(LayoutSession *session,
               QVector<LayoutPage> *pages,
               LayoutPage *page,
               int *pageIndex,
               qreal *y)
{
    if (session)
        session->finalizePage(page);
    pages->append(*page);
    ++(*pageIndex);
    *page = LayoutPage{.index = *pageIndex, .startDocPos = -1};
    *y = 0;
}

void notePageContent(LayoutPage *page, int docPos)
{
    if (page && page->startDocPos < 0 && docPos >= 0)
        page->startDocPos = docPos;
}

QTextCharFormat charFormatFromStyle(const CharStyle &style)
{
    QTextCharFormat cf;
    cf.setFont(style.font);
    if (style.bold)
        cf.setFontWeight(QFont::Bold);
    if (style.italic)
        cf.setFontItalic(true);
    if (style.underline)
        cf.setFontUnderline(true);
    if (style.superscript)
        cf.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    cf.setForeground(style.foreground);
    if (style.background.isValid())
        cf.setBackground(style.background);
    return cf;
}

void buildTextAndFormats(const QVector<DocRun> &runs,
                         int headingLevel,
                         QString *text,
                         QVector<QTextLayout::FormatRange> *ranges,
                         QFont *baseFont,
                         QVector<FootnoteMarker> *markers)
{
    bool haveBase = false;
    int pos = 0;
    for (const DocRun &run : runs) {
        if (run.isAtomic)
            continue;
        if (!haveBase) {
            *baseFont = run.style.font;
            haveBase = true;
        }
        QTextLayout::FormatRange fr;
        fr.start = pos;
        fr.length = run.text.size();
        fr.format = charFormatFromStyle(run.style);
        ranges->append(fr);
        if (markers && !run.footnoteId.isEmpty()) {
            FootnoteMarker m;
            m.textStart = pos;
            m.textLength = run.text.size();
            m.id = run.footnoteId;
            m.number = run.footnoteNumber;
            markers->append(m);
        }
        *text += run.text;
        pos += run.text.size();
    }

    if (!haveBase)
        baseFont->setPointSizeF(12.0);

    if (headingLevel > 0 && baseFont->pointSizeF() < 14.0) {
        static const qreal kHeadingSizes[] = {0, 18, 15, 13, 12};
        const int lvl = qBound(1, headingLevel, 4);
        baseFont->setPointSizeF(kHeadingSizes[lvl]);
        baseFont->setBold(true);
        for (QTextLayout::FormatRange &fr : *ranges) {
            QFont f = fr.format.font();
            if (f.pointSizeF() < 14.0) {
                f.setPointSizeF(kHeadingSizes[lvl]);
                f.setBold(true);
                fr.format.setFont(f);
                fr.format.setFontWeight(QFont::Bold);
            }
        }
    }
}

void appendLaidOutText(const QString &text,
                       const QFont &baseFont,
                       const QVector<QTextLayout::FormatRange> &ranges,
                       const QVector<FootnoteMarker> &markers,
                       LayoutSession *session,
                       int baseDocPos,
                       LayoutPage *page,
                       qreal *y,
                       QVector<LayoutPage> *pages,
                       int *pageIndex,
                       qreal *wrapBottomY,
                       qreal wrapTextX,
                       qreal wrapTextW)
{
    const qreal contentWidth = session->contentWidth;
    auto currentWidth = [&]() -> qreal {
        if (wrapBottomY && *wrapBottomY >= 0 && *y < *wrapBottomY && wrapTextW > 8.0)
            return wrapTextW;
        return contentWidth;
    };
    auto currentX = [&]() -> qreal {
        if (wrapBottomY && *wrapBottomY >= 0 && *y < *wrapBottomY && wrapTextW > 8.0)
            return wrapTextX;
        return 0;
    };
    auto clearWrapIfPast = [&]() {
        if (wrapBottomY && *wrapBottomY >= 0 && *y >= *wrapBottomY)
            *wrapBottomY = -1;
    };
    auto ensureFits = [&](qreal h) {
        if (*y + h > session->usableHeight() && *y > 0.01) {
            flushPage(session, pages, page, pageIndex, y);
            // Float wrap is page-local; do not carry leftover wrap height onto the next page.
            if (wrapBottomY)
                *wrapBottomY = -1;
        }
    };

    if (text.isEmpty()) {
        QTextLayout probe(QStringLiteral(" "), baseFont);
        probe.beginLayout();
        QTextLine line = probe.createLine();
        const qreal avail = currentWidth();
        line.setLineWidth(avail);
        const qreal h = line.isValid() ? line.height() : 14.0;
        probe.endLayout();
        ensureFits(h);
        notePageContent(page, baseDocPos);
        LayoutLine blank;
        blank.baseFont = baseFont;
        blank.x = currentX();
        blank.y = *y;
        blank.width = avail;
        blank.height = h;
        blank.ascent = h * 0.8;
        page->lines.append(blank);
        *y += h;
        clearWrapIfPast();
        return;
    }

    QTextLayout layout(text, baseFont);
    layout.setFormats(ranges);
    layout.setCacheEnabled(true);
    layout.beginLayout();
    qreal lineYInPara = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        const qreal avail = currentWidth();
        const qreal xOff = currentX();
        line.setLineWidth(avail);
        line.setPosition(QPointF(0, lineYInPara));
        const qreal h = qMax(1.0, line.height());

        // Claim footnotes for this line; if the line no longer fits, roll back claims,
        // flush prior content (with its footnotes), then re-claim on the new page.
        const LayoutSession::Snapshot beforeNotes = session->snapshot();
        session->claimInRange(markers, line.textStart(), line.textLength());
        if (*y + h > session->usableHeight() && *y > 0.01) {
            session->restore(beforeNotes);
            flushPage(session, pages, page, pageIndex, y);
            if (wrapBottomY)
                *wrapBottomY = -1;
            session->claimInRange(markers, line.textStart(), line.textLength());
        }

        const int lineDocPos = baseDocPos >= 0 ? baseDocPos + line.textStart() : -1;
        notePageContent(page, lineDocPos);

        LayoutLine out;
        out.text = text.mid(line.textStart(), line.textLength());
        out.baseFont = baseFont;
        for (const QTextLayout::FormatRange &fr : ranges) {
            const int lineStart = line.textStart();
            const int lineEnd = lineStart + line.textLength();
            const int clippedStart = qMax(fr.start, lineStart);
            const int clippedEnd = qMin(fr.start + fr.length, lineEnd);
            if (clippedStart >= clippedEnd)
                continue;
            QTextLayout::FormatRange local = fr;
            local.start = clippedStart - lineStart;
            local.length = clippedEnd - clippedStart;
            out.formats.append(local);
        }
        out.x = xOff;
        out.y = *y;
        out.width = line.naturalTextWidth();
        out.height = h;
        out.ascent = line.ascent();
        page->lines.append(out);
        *y += h;
        lineYInPara += h;
        clearWrapIfPast();
    }
    layout.endLayout();
}

void layoutAtomic(const DocRun &run,
                  LayoutSession *session,
                  int docPos,
                  LayoutPage *page,
                  qreal *y,
                  QVector<LayoutPage> *pages,
                  int *pageIndex,
                  qreal *wrapBottomY,
                  qreal *wrapTextX,
                  qreal *wrapTextW)
{
    const qreal contentWidth = session->contentWidth;
    qreal h = qMax(12.0, run.atomicHeightPt);
    qreal w = run.atomicWidthPt > 0 ? run.atomicWidthPt : contentWidth;
    w = qMin(w, contentWidth);
    if (*y + h > session->usableHeight() && *y > 0.01) {
        flushPage(session, pages, page, pageIndex, y);
        if (wrapBottomY)
            *wrapBottomY = -1;
    }
    // An atomic taller than the page cannot be moved to a "next page" (it never
    // fits); scale it down to the content box instead of overflowing the page.
    const qreal usableHeight = session->usableHeight();
    if (h > usableHeight) {
        const qreal k = usableHeight / h;
        h = usableHeight;
        w *= k;
    }
    notePageContent(page, docPos);

    const int wrap = run.imageWrap;
    const bool floatLeft = wrap == 2;
    const bool floatRight = wrap == 3;
    constexpr qreal kGap = 8.0;

    LayoutLine line;
    line.text = run.text;
    line.isAtomic = true;
    line.image = run.image;
    line.y = *y;
    line.width = w;
    line.height = h;
    line.ascent = h;

    if (floatLeft || floatRight) {
        line.x = floatLeft ? 0 : qMax(0.0, contentWidth - w);
        page->lines.append(line);
        if (wrapBottomY && wrapTextX && wrapTextW) {
            *wrapBottomY = *y + h;
            *wrapTextX = floatLeft ? (w + kGap) : 0;
            *wrapTextW = qMax(40.0, contentWidth - w - kGap);
        }
        return;
    }

    if (run.imageAlign & Qt::AlignRight)
        line.x = qMax(0.0, contentWidth - w);
    else if (run.imageAlign & Qt::AlignHCenter)
        line.x = qMax(0.0, (contentWidth - w) * 0.5);
    else
        line.x = 0;
    page->lines.append(line);
    *y += h;
}

void layoutParagraph(const DocParagraph &para,
                     LayoutSession *session,
                     LayoutPage *page,
                     qreal *y,
                     QVector<LayoutPage> *pages,
                     int *pageIndex)
{
    if (para.pageBreakBefore && *y > 0.01)
        flushPage(session, pages, page, pageIndex, y);

    qreal wrapBottomY = -1;
    qreal wrapTextX = 0;
    qreal wrapTextW = session->contentWidth;
    int docPos = para.documentPosition;

    QVector<DocRun> textBuf;
    auto flushText = [&]() {
        if (textBuf.isEmpty())
            return;
        QString text;
        QVector<QTextLayout::FormatRange> ranges;
        QFont baseFont;
        QVector<FootnoteMarker> markers;
        buildTextAndFormats(textBuf, para.headingLevel, &text, &ranges, &baseFont, &markers);
        appendLaidOutText(text, baseFont, ranges, markers, session, docPos, page, y, pages,
                          pageIndex, &wrapBottomY, wrapTextX, wrapTextW);
        docPos += text.size();
        textBuf.clear();
    };

    for (const DocRun &run : para.runs) {
        if (run.isAtomic) {
            flushText();
            layoutAtomic(run, session, docPos, page, y, pages, pageIndex, &wrapBottomY, &wrapTextX,
                         &wrapTextW);
            ++docPos;
        } else {
            textBuf.append(run);
        }
    }
    flushText();
    if (wrapBottomY >= 0 && *y < wrapBottomY)
        *y = wrapBottomY;
    *y += para.spaceAfterPt;
}

qreal measureWrappedHeight(const QString &text,
                           const QFont &baseFont,
                           const QVector<QTextLayout::FormatRange> &ranges,
                           qreal width)
{
    if (text.isEmpty()) {
        QTextLayout probe(QStringLiteral(" "), baseFont);
        probe.beginLayout();
        QTextLine line = probe.createLine();
        line.setLineWidth(width);
        const qreal h = line.isValid() ? line.height() : 14.0;
        probe.endLayout();
        return h;
    }
    QTextLayout layout(text, baseFont);
    layout.setFormats(ranges);
    layout.beginLayout();
    qreal total = 0;
    qreal y = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        total += line.height();
        y += line.height();
    }
    layout.endLayout();
    return qMax(14.0, total);
}

LayoutTableCell measureCell(const DocTableCell &cell, qreal width, qreal padding)
{
    LayoutTableCell out;
    out.background = cell.background;
    out.width = width;
    const qreal inner = qMax(8.0, width - 2 * padding);

    QString text;
    QVector<QTextLayout::FormatRange> ranges;
    QFont baseFont;
    baseFont.setPointSizeF(12.0);
    int pos = 0;
    bool haveBase = false;
    for (int pi = 0; pi < cell.paragraphs.size(); ++pi) {
        const DocParagraph &para = cell.paragraphs.at(pi);
        if (pi > 0) {
            text += QLatin1Char('\n');
            ++pos;
        }
        for (const DocRun &run : para.runs) {
            if (run.isAtomic) {
                text += QStringLiteral("[img]");
                pos += 5;
                continue;
            }
            if (!haveBase) {
                baseFont = run.style.font;
                haveBase = true;
            }
            QTextLayout::FormatRange fr;
            fr.start = pos;
            fr.length = run.text.size();
            fr.format = charFormatFromStyle(run.style);
            ranges.append(fr);
            text += run.text;
            pos += run.text.size();
        }
    }

    out.text = text;
    out.baseFont = baseFont;
    out.formats = ranges;
    out.height = measureWrappedHeight(text, baseFont, ranges, inner) + 2 * padding;
    return out;
}

void layoutTable(const DocTable &table,
                 LayoutSession *session,
                 LayoutPage *page,
                 qreal *y,
                 QVector<LayoutPage> *pages,
                 int *pageIndex)
{
    if (table.columnCount <= 0 || table.rows.isEmpty())
        return;

    const qreal contentWidth = session->contentWidth;
    QVector<qreal> weights = table.columnWeights;
    if (weights.size() != table.columnCount) {
        weights = QVector<qreal>(table.columnCount, 1.0);
    }
    qreal sum = 0;
    for (qreal w : weights)
        sum += w;
    if (sum <= 0)
        sum = table.columnCount;

    QVector<qreal> colW(table.columnCount);
    for (int c = 0; c < table.columnCount; ++c)
        colW[c] = contentWidth * (weights[c] / sum);

    const int rowCount = table.rows.size();
    QVector<qreal> rowH(rowCount, 0.0);
    struct AnchorMeasure {
        int r = 0;
        int c = 0;
        int colSpan = 1;
        int rowSpan = 1;
        qreal height = 0;
        LayoutTableCell prototype;
    };
    QVector<AnchorMeasure> anchors;

    for (int r = 0; r < rowCount; ++r) {
        const auto &row = table.rows.at(r);
        for (int c = 0; c < table.columnCount; ++c) {
            const DocTableCell &src = c < row.size() ? row.at(c) : DocTableCell{};
            if (src.covered)
                continue;
            const int colSpan = qMax(1, src.columnSpan);
            const int rowSpan = qMax(1, src.rowSpan);
            qreal w = 0;
            for (int k = 0; k < colSpan && c + k < table.columnCount; ++k)
                w += colW[c + k];
            LayoutTableCell measured = measureCell(src, w, table.cellPaddingPt);
            AnchorMeasure am;
            am.r = r;
            am.c = c;
            am.colSpan = colSpan;
            am.rowSpan = rowSpan;
            am.height = measured.height;
            am.prototype = measured;
            anchors.append(am);
            if (rowSpan == 1)
                rowH[r] = qMax(rowH[r], measured.height);
        }
        if (r < table.rowMinHeightsPt.size())
            rowH[r] = qMax(rowH[r], table.rowMinHeightsPt.at(r));
        rowH[r] = qMax(rowH[r], 14.0);
    }

    // Ensure multi-row spans fit into the sum of their row strips.
    for (const AnchorMeasure &am : anchors) {
        if (am.rowSpan <= 1)
            continue;
        qreal have = 0;
        for (int k = 0; k < am.rowSpan && am.r + k < rowCount; ++k)
            have += rowH[am.r + k];
        if (am.height > have + 0.5) {
            const qreal extra = am.height - have;
            const int last = qMin(rowCount - 1, am.r + am.rowSpan - 1);
            rowH[last] += extra;
        }
    }

    for (int r = 0; r < rowCount; ++r) {
        // Keep rowspan blocks together when possible.
        qreal blockH = rowH[r];
        int blockRows = 1;
        for (const AnchorMeasure &am : anchors) {
            if (am.r == r && am.rowSpan > 1) {
                qreal spanH = 0;
                for (int k = 0; k < am.rowSpan && r + k < rowCount; ++k)
                    spanH += rowH[r + k];
                if (spanH > blockH) {
                    blockH = spanH;
                    blockRows = am.rowSpan;
                }
            }
        }
        if (*y + blockH > session->usableHeight() && *y > 0.01)
            flushPage(session, pages, page, pageIndex, y);

        QVector<LayoutTableCell> cells;
        for (int c = 0; c < table.columnCount; ) {
            const DocTableCell &src =
                c < table.rows[r].size() ? table.rows[r][c] : DocTableCell{};
            if (src.covered) {
                LayoutTableCell skip;
                skip.covered = true;
                skip.width = colW[c];
                skip.height = rowH[r];
                skip.paintHeight = rowH[r];
                cells.append(skip);
                ++c;
                continue;
            }
            const int colSpan = qBound(1, src.columnSpan, table.columnCount - c);
            const int rowSpan = qBound(1, src.rowSpan, rowCount - r);
            LayoutTableCell cell;
            for (const AnchorMeasure &am : anchors) {
                if (am.r == r && am.c == c) {
                    cell = am.prototype;
                    break;
                }
            }
            cell.columnSpan = colSpan;
            cell.rowSpan = rowSpan;
            cell.width = 0;
            for (int k = 0; k < colSpan; ++k)
                cell.width += colW[c + k];
            cell.height = rowH[r];
            cell.paintHeight = 0;
            for (int k = 0; k < rowSpan && r + k < rowCount; ++k)
                cell.paintHeight += rowH[r + k];
            cells.append(cell);
            c += colSpan;
        }

        int rowDocPos = table.documentPosition;
        if (!table.rows[r].isEmpty() && !table.rows[r].first().paragraphs.isEmpty())
            rowDocPos = table.rows[r].first().paragraphs.first().documentPosition;
        notePageContent(page, rowDocPos);

        LayoutLine line;
        line.isTableRow = true;
        line.tableCells = cells;
        line.tableBorderPt = table.borderPt;
        line.tableBorderColor = table.borderColor;
        line.tableCellPaddingPt = table.cellPaddingPt;
        line.y = *y;
        line.width = contentWidth;
        line.height = rowH[r];
        line.ascent = rowH[r];
        page->lines.append(line);
        *y += rowH[r];
        Q_UNUSED(blockRows);
    }
    *y += 8.0;
}

void layoutBlock(const DocBlock &block,
                 LayoutSession *session,
                 LayoutPage *page,
                 qreal *y,
                 QVector<LayoutPage> *pages,
                 int *pageIndex)
{
    if (block.kind == DocBlock::Kind::Table)
        layoutTable(block.table, session, page, y, pages, pageIndex);
    else
        layoutParagraph(block.paragraph, session, page, y, pages, pageIndex);
}

DocParagraph makePlainParagraph(const QString &text, qreal pointSize, bool bold, int headingLevel)
{
    DocParagraph para;
    para.headingLevel = headingLevel;
    if (headingLevel == 1)
        para.spaceAfterPt = 12.0;
    else if (headingLevel >= 2)
        para.spaceAfterPt = 8.0;
    else
        para.spaceAfterPt = 4.0;
    DocRun run;
    run.text = text;
    run.style.font.setPointSizeF(pointSize);
    run.style.bold = bold;
    if (bold)
        run.style.font.setBold(true);
    para.runs.append(run);
    return para;
}

void layoutEndnotesSection(LayoutSession *session,
                           LayoutPage *page,
                           qreal *y,
                           QVector<LayoutPage> *pages,
                           int *pageIndex)
{
    if (!session || !session->model || session->model->endnoteOrder.isEmpty())
        return;

    // Document-end notes start on a fresh page when body content exists.
    if (*y > 0.01 || !page->lines.isEmpty() || !pages->isEmpty())
        flushPage(session, pages, page, pageIndex, y);

    DocParagraph heading = makePlainParagraph(QStringLiteral("尾注"), 14.0, true, 2);
    heading.pageBreakBefore = false;
    layoutParagraph(heading, session, page, y, pages, pageIndex);

    int n = 1;
    for (const QString &id : session->model->endnoteOrder) {
        const QString body = session->model->endnoteBodies.value(id);
        const QString line = QStringLiteral("%1. %2").arg(n).arg(body);
        DocParagraph para = makePlainParagraph(line, 10.0, false, 0);
        layoutParagraph(para, session, page, y, pages, pageIndex);
        ++n;
    }
}

} // namespace

LayoutResult LayoutEngine::layout(const DocumentModel &model, const PageLayoutSettings &fallbackSetup)
{
    LayoutResult result;
    const PageLayoutSettings active = fallbackSetup;
    const QRectF box = contentRectPoints(active);
    result.contentWidthPt = box.width();
    result.contentHeightPt = box.height();

    const bool hasBody = !model.sections.isEmpty() && model.blockCount() > 0;
    const bool hasEndnotes = !model.endnoteOrder.isEmpty();
    if (!hasBody && !hasEndnotes) {
        result.pages.append(LayoutPage{.index = 0});
        if (!model.floatingBoxes.isEmpty()) {
            int maxPage = 0;
            for (const FloatingTextBox &box : model.floatingBoxes)
                maxPage = qMax(maxPage, box.pageIndex);
            while (result.pages.size() <= maxPage) {
                LayoutPage extra;
                extra.index = result.pages.size();
                result.pages.append(extra);
            }
            for (const FloatingTextBox &box : model.floatingBoxes) {
                const int idx = qBound(0, box.pageIndex, result.pages.size() - 1);
                result.pages[idx].floatingBoxes.append(box);
            }
        }
        return result;
    }

    LayoutSession session;
    session.model = &model;
    session.fullHeight = box.height();
    session.contentWidth = box.width();

    int pageIndex = 0;
    LayoutPage page{.index = 0, .startDocPos = -1};
    qreal y = 0;

    if (hasBody) {
        for (const DocSection &section : model.sections) {
            if (!pageSetupEquals(section.pageSetup, active)) {
                // A section with its own paper/margins starts on a fresh page
                // using that section's content box.
                if (y > 0.01 || !page.lines.isEmpty())
                    flushPage(&session, &result.pages, &page, &pageIndex, &y);
                const QRectF sectionBox = contentRectPoints(section.pageSetup);
                session.fullHeight = sectionBox.height();
                session.contentWidth = sectionBox.width();
            }
            for (const DocBlock &block : section.blocks)
                layoutBlock(block, &session, &page, &y, &result.pages, &pageIndex);
        }
    }

    layoutEndnotesSection(&session, &page, &y, &result.pages, &pageIndex);

    session.finalizePage(&page);
    result.pages.append(page);

    // Attach absolute floating boxes to their target pages (create empty pages if needed).
    if (!model.floatingBoxes.isEmpty()) {
        int maxPage = result.pages.isEmpty() ? 0 : result.pages.last().index;
        for (const FloatingTextBox &box : model.floatingBoxes)
            maxPage = qMax(maxPage, box.pageIndex);
        while (result.pages.size() <= maxPage) {
            LayoutPage extra;
            extra.index = result.pages.size();
            result.pages.append(extra);
        }
        for (const FloatingTextBox &box : model.floatingBoxes) {
            const int idx = qBound(0, box.pageIndex, result.pages.size() - 1);
            result.pages[idx].floatingBoxes.append(box);
        }
    }

    return result;
}

int LayoutEngine::pageCount(const DocumentModel &model, const PageLayoutSettings &fallbackSetup)
{
    return layout(model, fallbackSetup).pageCount();
}

} // namespace Engine
