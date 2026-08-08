#include "mainwindow.h"
#include "documenttab.h"
#include "lazyfontcombobox.h"
#include "styleutils.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QTextCursor>
#include <QTextList>
#include <QStatusBar>
#include <QTextEdit>

void MainWindow::textBold()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(m_actionBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(m_actionItalic->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(m_actionUnderline->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textStrikeout()
{
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(m_actionStrike->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textSuperScript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment(m_actionSuper->isChecked()
                                 ? QTextCharFormat::AlignSuperScript
                                 : QTextCharFormat::AlignNormal);
    if (m_actionSuper->isChecked())
        m_actionSub->setChecked(false);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textSubScript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment(m_actionSub->isChecked()
                                 ? QTextCharFormat::AlignSubScript
                                 : QTextCharFormat::AlignNormal);
    if (m_actionSub->isChecked())
        m_actionSuper->setChecked(false);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textFamily(const QString &family)
{
    QTextCharFormat fmt;
    fmt.setFontFamilies({family});
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textSize(const QString &size)
{
    bool ok = false;
    const qreal pointSize = size.toDouble(&ok);
    if (!ok || pointSize <= 0)
        return;
    QTextCharFormat fmt;
    fmt.setFontPointSize(pointSize);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textColor()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    const QColor color = QColorDialog::getColor(editor->textColor(), this, tr("文字颜色"));
    if (!color.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setForeground(color);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textHighlight()
{
    const QColor color = QColorDialog::getColor(Qt::yellow, this, tr("高亮颜色"));
    if (!color.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setBackground(color);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::clearFormatting()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    QTextCharFormat plain;
    plain.setFont(QFont(QStringLiteral("PingFang SC"), 12));
    plain.setForeground(Qt::black);
    plain.setBackground(Qt::transparent);
    plain.setVerticalAlignment(QTextCharFormat::AlignNormal);
    cursor.setCharFormat(plain);
    QTextBlockFormat block;
    block.setHeadingLevel(0);
    cursor.mergeBlockFormat(block);
    editor->setCurrentCharFormat(plain);
}

void MainWindow::setHeading(int level)
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    StyleUtils::applyHeadingLevel(cursor, level);
    editor->setTextCursor(cursor);
    updateStyleActions();
}

void MainWindow::applyDocumentStyle()
{
    auto *editor = currentEditor();
    auto *action = qobject_cast<QAction *>(sender());
    if (!editor || !action)
        return;
    QTextCursor cursor = editor->textCursor();
    StyleUtils::applyStyle(cursor, StyleUtils::StyleId(action->data().toInt()));
    editor->setTextCursor(cursor);
    editor->mergeCurrentCharFormat(cursor.block().charFormat());
    updateStyleActions();
}

void MainWindow::updateStyleActions()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    const StyleUtils::StyleId id = StyleUtils::detectStyle(editor->textCursor());
    for (QAction *a : m_styleActionGroup->actions()) {
        if (a->data().toInt() == int(id)) {
            a->setChecked(true);
            break;
        }
    }
}

void MainWindow::toggleBulletList()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextList *list = cursor.currentList();
    if (list && list->format().style() == QTextListFormat::ListDisc) {
        cursor.beginEditBlock();
        list->remove(cursor.block());
        QTextBlockFormat plain;
        plain.setIndent(0);
        cursor.setBlockFormat(plain);
        cursor.endEditBlock();
        m_actionBullet->setChecked(false);
        return;
    }
    QTextListFormat listFmt;
    listFmt.setStyle(QTextListFormat::ListDisc);
    cursor.createList(listFmt);
    m_actionBullet->setChecked(true);
    m_actionNumbered->setChecked(false);
}

void MainWindow::toggleNumberedList()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextList *list = cursor.currentList();
    if (list && list->format().style() == QTextListFormat::ListDecimal) {
        cursor.beginEditBlock();
        if (cursor.currentList())
            cursor.currentList()->remove(cursor.block());
        QTextBlockFormat plain;
        plain.setIndent(0);
        cursor.setBlockFormat(plain);
        cursor.endEditBlock();
        m_actionNumbered->setChecked(false);
        return;
    }
    QTextListFormat listFmt;
    listFmt.setStyle(QTextListFormat::ListDecimal);
    cursor.createList(listFmt);
    m_actionNumbered->setChecked(true);
    m_actionBullet->setChecked(false);
}

void MainWindow::increaseIndent()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setIndent(fmt.indent() + 1);
    cursor.setBlockFormat(fmt);
}

void MainWindow::decreaseIndent()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setIndent(qMax(0, fmt.indent() - 1));
    cursor.setBlockFormat(fmt);
}

void MainWindow::setLineSpacing(qreal factor)
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setLineHeight(factor * 100.0, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(fmt);
}

void MainWindow::increaseFontSize()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    qreal size = editor->currentFont().pointSizeF();
    if (size <= 0)
        size = 12;
    textSize(QString::number(size + 1));
}

void MainWindow::decreaseFontSize()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    qreal size = editor->currentFont().pointSizeF();
    if (size <= 0)
        size = 12;
    if (size > 6)
        textSize(QString::number(size - 1));
}

void MainWindow::toUpperCase() { changeCase(0); }
void MainWindow::toLowerCase() { changeCase(1); }
void MainWindow::toTitleCase() { changeCase(2); }

void MainWindow::changeCase(int mode)
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    QString text = cursor.selectedText();
    if (text.isEmpty())
        return;
    if (mode == 0)
        text = text.toUpper();
    else if (mode == 1)
        text = text.toLower();
    else {
        QStringList words = text.split(QChar::Space, Qt::SkipEmptyParts);
        for (QString &w : words) {
            if (!w.isEmpty())
                w = w.at(0).toUpper() + w.mid(1).toLower();
        }
        text = words.join(QLatin1Char(' '));
    }
    cursor.insertText(text);
}

void MainWindow::pastePlainText()
{
    if (auto *e = currentEditor())
        e->textCursor().insertText(QApplication::clipboard()->text());
}

void MainWindow::pickFormat()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    m_copiedCharFormat = cursor.charFormat();
    m_copiedBlockFormat = cursor.blockFormat();
    m_hasCopiedFormat = true;
    m_formatPainterArmed = true;
    m_actionFormatPainter->setChecked(true);
    statusBar()->showMessage(tr("格式刷：请选择要应用格式的文本"), 4000);
}

void MainWindow::applyPickedFormat()
{
    if (!m_hasCopiedFormat || !m_formatPainterArmed)
        return;
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection())
        return;
    cursor.mergeCharFormat(m_copiedCharFormat);
    cursor.mergeBlockFormat(m_copiedBlockFormat);
    m_formatPainterArmed = false;
    m_actionFormatPainter->setChecked(false);
    statusBar()->showMessage(tr("已应用格式"), 2000);
}

void MainWindow::mergeFormatOnWordOrSelection(const QTextCharFormat &format)
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    editor->mergeCurrentCharFormat(format);
}

void MainWindow::fontChanged(const QFont &font)
{
    m_fontCombo->blockSignals(true);
    m_fontCombo->setCurrentFont(font);
    m_fontCombo->blockSignals(false);
    m_sizeCombo->blockSignals(true);
    m_sizeCombo->setCurrentText(QString::number(font.pointSize() > 0 ? font.pointSize() : 12));
    m_sizeCombo->blockSignals(false);
    m_actionBold->setChecked(font.bold());
    m_actionItalic->setChecked(font.italic());
    m_actionUnderline->setChecked(font.underline());
    m_actionStrike->setChecked(font.strikeOut());
}

void MainWindow::alignmentChanged(Qt::Alignment alignment)
{
    if (alignment & Qt::AlignLeft)
        m_actionAlignLeft->setChecked(true);
    else if (alignment & Qt::AlignHCenter)
        m_actionAlignCenter->setChecked(true);
    else if (alignment & Qt::AlignRight)
        m_actionAlignRight->setChecked(true);
    else if (alignment & Qt::AlignJustify)
        m_actionAlignJustify->setChecked(true);
}

void MainWindow::updateListActions()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextList *list = editor->textCursor().currentList();
    m_actionBullet->setChecked(list && list->format().style() == QTextListFormat::ListDisc);
    m_actionNumbered->setChecked(list && list->format().style() == QTextListFormat::ListDecimal);
}
