#include "tablegeometry.h"

#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextLength>
#include <QTextTable>
#include <QTextTableCell>
#include <QTextTableCellFormat>
#include <QTextTableFormat>

namespace TableGeometry {

QVector<qreal> columnWidthPercents(const QTextTable *table)
{
    QVector<qreal> out;
    if (!table || table->columns() <= 0)
        return out;

    const int cols = table->columns();
    out.resize(cols);
    const QVector<QTextLength> constraints = table->format().columnWidthConstraints();
    qreal sum = 0;
    bool any = false;
    for (int c = 0; c < cols; ++c) {
        qreal pct = 100.0 / cols;
        if (c < constraints.size()) {
            const QTextLength &len = constraints.at(c);
            if (len.type() == QTextLength::PercentageLength) {
                pct = qMax(0.1, len.rawValue());
                any = true;
            } else if (len.type() == QTextLength::FixedLength) {
                pct = qMax(0.1, len.rawValue());
                any = true;
            }
        }
        out[c] = pct;
        sum += pct;
    }
    if (!any || sum <= 0) {
        const qreal even = 100.0 / cols;
        for (int c = 0; c < cols; ++c)
            out[c] = even;
        return out;
    }
    // Normalize FixedLength mixes and rounding drift to percentages.
    for (int c = 0; c < cols; ++c)
        out[c] = out[c] * 100.0 / sum;
    return out;
}

void setColumnWidthPercents(QTextTable *table, const QVector<qreal> &percents)
{
    if (!table || table->columns() <= 0 || percents.size() != table->columns())
        return;

    qreal sum = 0;
    for (qreal p : percents)
        sum += qMax(0.0, p);
    if (sum <= 0)
        return;

    QTextTableFormat fmt = table->format();
    QList<QTextLength> constraints;
    constraints.reserve(percents.size());
    for (qreal p : percents) {
        const qreal pct = qMax(0.1, p) * 100.0 / sum;
        constraints << QTextLength(QTextLength::PercentageLength, pct);
    }
    fmt.setColumnWidthConstraints(constraints);
    table->setFormat(fmt);
}

QVector<qreal> columnEdgeXs(const QTextTable *table, const QRectF &tableRect)
{
    QVector<qreal> edges;
    if (!table || table->columns() <= 0 || tableRect.width() <= 1.0)
        return edges;

    const QVector<qreal> percents = columnWidthPercents(table);
    edges.reserve(percents.size() + 1);
    edges.append(tableRect.left());
    qreal x = tableRect.left();
    for (qreal pct : percents) {
        x += tableRect.width() * (pct / 100.0);
        edges.append(x);
    }
    if (!edges.isEmpty())
        edges.last() = tableRect.right();
    return edges;
}

qreal rowMinHeightPt(const QTextTable *table, int row)
{
    if (!table || row < 0 || row >= table->rows() || table->columns() <= 0)
        return 0;

    qreal best = 0;
    for (int c = 0; c < table->columns(); ++c) {
        const QTextTableCell cell = table->cellAt(row, c);
        const QTextCharFormat fmt = cell.format();
        if (fmt.hasProperty(RowMinHeightProperty))
            best = qMax(best, fmt.property(RowMinHeightProperty).toReal());
    }
    if (best > 0)
        return best;

    // Fallback: first cell's first block MinimumHeight.
    const QTextTableCell cell = table->cellAt(row, 0);
    const QTextBlock block = cell.firstCursorPosition().block();
    const QTextBlockFormat bf = block.blockFormat();
    if (bf.lineHeightType() == QTextBlockFormat::MinimumHeight)
        return bf.lineHeight();
    return 0;
}

void setRowMinHeightPt(QTextTable *table, int row, qreal heightPt)
{
    if (!table || row < 0 || row >= table->rows())
        return;

    const qreal h = heightPt > 0.5 ? heightPt : 0.0;
    for (int c = 0; c < table->columns(); ++c) {
        QTextTableCell cell = table->cellAt(row, c);
        QTextTableCellFormat cellFmt = cell.format().toTableCellFormat();
        if (h > 0)
            cellFmt.setProperty(RowMinHeightProperty, h);
        else
            cellFmt.clearProperty(RowMinHeightProperty);
        cell.setFormat(cellFmt);

        QTextCursor cur = cell.firstCursorPosition();
        QTextBlockFormat bf = cur.blockFormat();
        if (h > 0) {
            bf.setLineHeight(h, QTextBlockFormat::MinimumHeight);
        } else {
            bf.setLineHeight(100, QTextBlockFormat::ProportionalHeight);
        }
        cur.mergeBlockFormat(bf);
    }
}

} // namespace TableGeometry
