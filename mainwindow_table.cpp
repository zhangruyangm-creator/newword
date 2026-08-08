#include "mainwindow.h"
#include "documenttab.h"
#include "tabledialogs.h"
#include "tablegeometry.h"

#include <QAction>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextTable>

void MainWindow::insertTable()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    InsertTableDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    QTextTable *table = cursor.insertTable(dialog.rowCount(), dialog.columnCount(),
                                           dialog.tableFormat());
    if (table && dialog.withHeaderRow())
        applyHeaderStyleToRow(table, 0, true);
    cursor.endEditBlock();
    updateTableActions();
}

QTextTable *MainWindow::currentTable() const
{
    auto *editor = currentEditor();
    return editor ? editor->textCursor().currentTable() : nullptr;
}

QTextTableCell MainWindow::currentTableCell() const
{
    auto *editor = currentEditor();
    if (!editor)
        return {};
    QTextTable *table = editor->textCursor().currentTable();
    if (!table)
        return {};
    return table->cellAt(editor->textCursor());
}

void MainWindow::applyHeaderStyleToRow(QTextTable *table, int row, bool enable)
{
    if (!table || row < 0 || row >= table->rows())
        return;
    for (int c = 0; c < table->columns(); ++c) {
        QTextTableCell cell = table->cellAt(row, c);
        QTextCursor cursor = cell.firstCursorPosition();
        cursor.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
        QTextCharFormat fmt;
        if (enable) {
            fmt.setFontWeight(QFont::Bold);
            fmt.setBackground(QColor(QStringLiteral("#D6E3F3")));
        } else {
            fmt.setFontWeight(QFont::Normal);
            fmt.setBackground(Qt::transparent);
        }
        cursor.mergeCharFormat(fmt);

        QTextTableCellFormat cellFmt = cell.format().toTableCellFormat();
        if (enable)
            cellFmt.setBackground(QColor(QStringLiteral("#D6E3F3")));
        else
            cellFmt.clearBackground();
        cell.setFormat(cellFmt);
    }
}

void MainWindow::insertTableRow()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid())
        table->insertRows(cell.row() + 1, 1);
}

void MainWindow::insertTableRowAbove()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid())
        table->insertRows(cell.row(), 1);
}

void MainWindow::insertTableColumn()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid())
        table->insertColumns(cell.column() + 1, 1);
}

void MainWindow::insertTableColumnLeft()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid())
        table->insertColumns(cell.column(), 1);
}

void MainWindow::removeTableRow()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid() && table->rows() > 1)
        table->removeRows(cell.row(), 1);
}

void MainWindow::removeTableColumn()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (table && cell.isValid() && table->columns() > 1)
        table->removeColumns(cell.column(), 1);
}

void MainWindow::deleteTable()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    if (!editor || !table)
        return;
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(table->firstPosition());
    cursor.setPosition(table->lastPosition() + 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    updateTableActions();
}

void MainWindow::splitTableCells()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (!table || !cell.isValid())
        return;

    bool okRows = false;
    const int rows = QInputDialog::getInt(this, tr("拆分单元格"), tr("拆分为行数:"),
                                          1, 1, 10, 1, &okRows);
    if (!okRows)
        return;
    bool okCols = false;
    const int cols = QInputDialog::getInt(this, tr("拆分单元格"), tr("拆分为列数:"),
                                          2, 1, 10, 1, &okCols);
    if (!okCols)
        return;
    if (rows * cols <= 1)
        return;
    table->splitCell(cell.row(), cell.column(), rows, cols);
}

void MainWindow::selectTableRow()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (!editor || !table || !cell.isValid())
        return;
    QTextTableCell first = table->cellAt(cell.row(), 0);
    QTextTableCell last = table->cellAt(cell.row(), table->columns() - 1);
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(first.firstPosition());
    cursor.setPosition(last.lastPosition(), QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}

void MainWindow::selectTableColumn()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (!editor || !table || !cell.isValid())
        return;
    QTextTableCell first = table->cellAt(0, cell.column());
    QTextTableCell last = table->cellAt(table->rows() - 1, cell.column());
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(first.firstPosition());
    cursor.setPosition(last.lastPosition(), QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}

void MainWindow::selectTable()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    if (!editor || !table)
        return;
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(table->firstPosition());
    cursor.setPosition(table->lastPosition(), QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}

void MainWindow::setTableCellBackground()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    if (!editor || !table)
        return;

    const QColor color = QColorDialog::getColor(QColor(QStringLiteral("#FFF2CC")), this,
                                                tr("单元格底色"));
    if (!color.isValid())
        return;

    QTextCursor cursor = editor->textCursor();
    QTextTableCell first = table->cellAt(cursor.selectionStart());
    QTextTableCell last = table->cellAt(cursor.selectionEnd());
    if (!first.isValid())
        first = table->cellAt(cursor);
    if (!last.isValid())
        last = first;
    if (!first.isValid())
        return;

    const int rowMin = qMin(first.row(), last.row());
    const int rowMax = qMax(first.row(), last.row());
    const int colMin = qMin(first.column(), last.column());
    const int colMax = qMax(first.column(), last.column());

    cursor.beginEditBlock();
    for (int r = rowMin; r <= rowMax; ++r) {
        for (int c = colMin; c <= colMax; ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            QTextTableCellFormat cellFmt = cell.format().toTableCellFormat();
            cellFmt.setBackground(color);
            cell.setFormat(cellFmt);
        }
    }
    cursor.endEditBlock();
}

void MainWindow::evenTableColumns()
{
    QTextTable *table = currentTable();
    if (!table || table->columns() <= 0)
        return;
    QVector<qreal> percents(table->columns(), 100.0 / table->columns());
    TableGeometry::setColumnWidthPercents(table, percents);
}

void MainWindow::editTableColumnWidths()
{
    QTextTable *table = currentTable();
    if (!table || table->columns() <= 0)
        return;
    const QTextTableCell cell = currentTableCell();
    const int highlight = cell.isValid() ? cell.column() : -1;
    ColumnWidthsDialog dialog(TableGeometry::columnWidthPercents(table), highlight, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TableGeometry::setColumnWidthPercents(table, dialog.percents());
}

void MainWindow::editTableRowHeight()
{
    QTextTable *table = currentTable();
    QTextTableCell cell = currentTableCell();
    if (!table || !cell.isValid())
        return;

    const int row = cell.row();
    RowHeightDialog dialog(TableGeometry::rowMinHeightPt(table, row), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const qreal height = dialog.heightPt();
    if (dialog.applyToAllRows()) {
        for (int r = 0; r < table->rows(); ++r)
            TableGeometry::setRowMinHeightPt(table, r, height);
    } else {
        TableGeometry::setRowMinHeightPt(table, row, height);
    }
}

void MainWindow::bandTableRows()
{
    QTextTable *table = currentTable();
    if (!table)
        return;
    const QColor band(QStringLiteral("#F2F2F2"));
    for (int r = 0; r < table->rows(); ++r) {
        for (int c = 0; c < table->columns(); ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            QTextTableCellFormat cellFmt = cell.format().toTableCellFormat();
            if (r % 2 == 1)
                cellFmt.setBackground(band);
            else
                cellFmt.clearBackground();
            cell.setFormat(cellFmt);
        }
    }
}

void MainWindow::tableProperties()
{
    QTextTable *table = currentTable();
    if (!table)
        return;
    TablePropertiesDialog dialog(table->format(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QTextTableFormat fmt = dialog.tableFormat();
    fmt.setColumnWidthConstraints(table->format().columnWidthConstraints());
    table->setFormat(fmt);
}

void MainWindow::mergeTableCells()
{
    auto *editor = currentEditor();
    QTextTable *table = currentTable();
    if (!editor || !table)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextTableCell first = table->cellAt(cursor.selectionStart());
    QTextTableCell last = table->cellAt(cursor.selectionEnd());
    if (!first.isValid() || !last.isValid()) {
        QMessageBox::information(this, tr("合并单元格"), tr("请先选中表格中的多个单元格。"));
        return;
    }
    const int row = qMin(first.row(), last.row());
    const int col = qMin(first.column(), last.column());
    const int numRows = qAbs(last.row() - first.row()) + 1;
    const int numCols = qAbs(last.column() - first.column()) + 1;
    if (numRows * numCols <= 1) {
        QMessageBox::information(this, tr("合并单元格"), tr("请选中至少两个单元格。"));
        return;
    }
    table->mergeCells(row, col, numRows, numCols);
}

void MainWindow::styleTableHeader()
{
    applyHeaderStyleToRow(currentTable(), 0, true);
}

void MainWindow::clearTableHeader()
{
    applyHeaderStyleToRow(currentTable(), 0, false);
}

void MainWindow::updateTableActions()
{
    const bool inTable = currentTable() != nullptr;
    m_actionTableAddRow->setEnabled(inTable);
    m_actionTableAddRowAbove->setEnabled(inTable);
    m_actionTableAddCol->setEnabled(inTable);
    m_actionTableAddColLeft->setEnabled(inTable);
    m_actionTableDelRow->setEnabled(inTable);
    m_actionTableDelCol->setEnabled(inTable);
    m_actionTableDelete->setEnabled(inTable);
    m_actionTableMerge->setEnabled(inTable);
    m_actionTableSplit->setEnabled(inTable);
    m_actionTableHeader->setEnabled(inTable);
    m_actionTableClearHeader->setEnabled(inTable);
    m_actionTableSelectRow->setEnabled(inTable);
    m_actionTableSelectCol->setEnabled(inTable);
    m_actionTableSelect->setEnabled(inTable);
    m_actionTableCellBg->setEnabled(inTable);
    m_actionTableEvenCols->setEnabled(inTable);
    m_actionTableColumnWidths->setEnabled(inTable);
    m_actionTableRowHeight->setEnabled(inTable);
    m_actionTableBandRows->setEnabled(inTable);
    m_actionTableProperties->setEnabled(inTable);
}
