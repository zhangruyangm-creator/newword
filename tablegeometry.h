#ifndef TABLEGEOMETRY_H
#define TABLEGEOMETRY_H

#include <QRectF>
#include <QTextFormat>
#include <QVector>

class QTextTable;

/** Column width / row height helpers for QTextTable. */
namespace TableGeometry {

constexpr int RowMinHeightProperty = QTextFormat::UserProperty + 110;
//! Marks a 1×1 table used as an inline text box.
constexpr int TextBoxProperty = QTextFormat::UserProperty + 111;

[[nodiscard]] QVector<qreal> columnWidthPercents(const QTextTable *table);
void setColumnWidthPercents(QTextTable *table, const QVector<qreal> &percents);

//! Document-space X coordinates of column edges (size = columnCount + 1).
[[nodiscard]] QVector<qreal> columnEdgeXs(const QTextTable *table, const QRectF &tableRect);

[[nodiscard]] qreal rowMinHeightPt(const QTextTable *table, int row);
void setRowMinHeightPt(QTextTable *table, int row, qreal heightPt);

} // namespace TableGeometry

#endif // TABLEGEOMETRY_H
