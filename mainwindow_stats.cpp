#include "mainwindow.h"
#include "documenttab.h"

#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>

void MainWindow::onDocumentModified(bool)
{
    updateTabTitle(qobject_cast<DocumentTab *>(sender()));
}

void MainWindow::onCursorMoved()
{
    auto *editor = currentEditor();
    if (!editor || !m_positionLabel)
        return;
    const QTextCursor cursor = editor->textCursor();
    QString text = tr("行 %1, 列 %2")
                       .arg(cursor.blockNumber() + 1)
                       .arg(cursor.positionInBlock() + 1);

    if (DocumentTab *tab = currentTab()) {
        if (tab->viewMode() == DocumentViewMode::Page && tab->pageBodyHeightPx() > 0) {
            text += tr(" · 第 %1 / %2 页")
                        .arg(tab->currentPageIndex() + 1)
                        .arg(tab->pageCount());
        }
    }
    m_positionLabel->setText(text);

    // Cheap updates first; style/list sync is still light enough for cursor moves.
    alignmentChanged(editor->alignment());
    updateListActions();
    updateStyleActions();

    // Table action enablement only matters near tables — skip when clearly outside.
    const bool inTable = cursor.currentTable() != nullptr;
    if (inTable || m_cursorWasInTable)
        updateTableActions();
    m_cursorWasInTable = inTable;

    // Full-doc stats are expensive; only refresh on cursor move when a selection exists.
    if (cursor.hasSelection())
        refreshStatsSoon();
}

void MainWindow::onContentsChanged()
{
    refreshStatsSoon();
    refreshOutlineSoon();
}

void MainWindow::refreshStatsSoon()
{
    if (!m_statsRefreshTimer)
        return;
    auto *editor = currentEditor();
    const int chars = editor && editor->document() ? editor->document()->characterCount() : 0;
    if (chars > 80000)
        m_statsRefreshTimer->setInterval(500);
    else if (chars > 25000)
        m_statsRefreshTimer->setInterval(350);
    else
        m_statsRefreshTimer->setInterval(250);
    m_statsRefreshTimer->start();
}

void MainWindow::updateDocumentStats()
{
    auto *editor = currentEditor();
    if (!editor || !m_wordCountLabel)
        return;

    QTextDocument *document = editor->document();
    TextStats::Counts docCounts;
    if (document && document->revision() == m_cachedStatsRevision && m_cachedStatsRevision >= 0) {
        docCounts = m_cachedDocStats;
    } else {
        docCounts = TextStats::analyze(editor->toPlainText());
        m_cachedDocStats = docCounts;
        m_cachedStatsRevision = document ? document->revision() : -1;
    }

    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        const TextStats::Counts sel = TextStats::analyze(cursor.selectedText());
        m_wordCountLabel->setText(
            tr("选中 字数:%1 字符:%2 标点:%3  |  全文 字数:%4 字符:%5")
                .arg(sel.words)
                .arg(sel.chars)
                .arg(sel.punctuation)
                .arg(docCounts.words)
                .arg(docCounts.chars));
        m_wordCountLabel->setToolTip(
            tr("选中：字数 %1，字符（不计空白）%2，字符（计空白）%3，标点 %4，汉字 %5\n"
               "全文：字数 %6，字符（不计空白）%7，字符（计空白）%8，标点 %9，汉字 %10")
                .arg(sel.words).arg(sel.chars).arg(sel.charsWithSpaces)
                .arg(sel.punctuation).arg(sel.cjkChars)
                .arg(docCounts.words).arg(docCounts.chars).arg(docCounts.charsWithSpaces)
                .arg(docCounts.punctuation).arg(docCounts.cjkChars));
    } else {
        m_wordCountLabel->setText(
            tr("字数:%1  字符:%2  标点:%3")
                .arg(docCounts.words)
                .arg(docCounts.chars)
                .arg(docCounts.punctuation));
        m_wordCountLabel->setToolTip(
            tr("字数 %1（汉字按字、英文按词）\n"
               "字符（不计空白）%2\n"
               "字符（计空白）%3\n"
               "标点 %4\n"
               "汉字 %5")
                .arg(docCounts.words)
                .arg(docCounts.chars)
                .arg(docCounts.charsWithSpaces)
                .arg(docCounts.punctuation)
                .arg(docCounts.cjkChars));
    }
}

void MainWindow::currentCharFormatChanged(const QTextCharFormat &format)
{
    fontChanged(format.font());
    m_actionSuper->setChecked(format.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    m_actionSub->setChecked(format.verticalAlignment() == QTextCharFormat::AlignSubScript);
}
