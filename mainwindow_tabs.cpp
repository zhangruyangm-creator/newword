#include "mainwindow.h"
#include "docxconverter.h"
#include "documenttab.h"
#include "findreplacedialog.h"
#include "lazyfontcombobox.h"
#include "outlinepane.h"
#include "pdfviewtab.h"
#include "spellchecker.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QMessageBox>
#include <QShowEvent>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

DocumentTab *MainWindow::currentTab() const
{
    return qobject_cast<DocumentTab *>(m_docTabs->currentWidget());
}

PdfViewTab *MainWindow::currentPdfTab() const
{
    return qobject_cast<PdfViewTab *>(m_docTabs->currentWidget());
}

PagedEditorWidget *MainWindow::currentEditor() const
{
    if (DocumentTab *tab = currentTab())
        return tab->editor();
    return nullptr;
}

DocumentTab *MainWindow::createTab(const QString &filePath)
{
    auto *tab = new DocumentTab(m_docTabs);
    tab->setSpellCheckEnabled(m_spellCheckEnabled && SpellChecker::isAvailable());
    if (m_actionShowRuler)
        tab->setRulerVisible(m_actionShowRuler->isChecked());
    if (m_actionShowGrid)
        tab->setGridLinesVisible(m_actionShowGrid->isChecked());
    if (!filePath.isEmpty()) {
        QString error;
        if (!tab->loadFromFile(filePath, &error)) {
            QMessageBox::warning(this, tr("打开失败"), error);
            tab->deleteLater();
            return nullptr;
        }
        if (QFileInfo(filePath).suffix().compare(QLatin1String("docx"), Qt::CaseInsensitive) == 0) {
            const QString note = DocxConverter::lastStatusNote();
            if (!note.isEmpty())
                statusBar()->showMessage(note, 5000);
        }
    } else {
        tab->applyEditorDefaults();
    }
    bindTab(tab);
    const int index = m_docTabs->addTab(tab, tab->displayName());
    m_docTabs->setCurrentIndex(index);
    updateTabTitle(tab);
    updateUiForCurrentTab();
    return tab;
}

PdfViewTab *MainWindow::createPdfTab(const QString &filePath)
{
    auto *tab = new PdfViewTab(m_docTabs);
    QString error;
    if (!tab->loadFile(filePath, &error)) {
        QMessageBox::warning(this, tr("打开失败"), error);
        tab->deleteLater();
        return nullptr;
    }
    connect(tab, &PdfViewTab::zoomChanged, this, &MainWindow::updateZoomUi);
    connect(tab, &PdfViewTab::pageChanged, this, [this](int page, int total) {
        if (m_positionLabel)
            m_positionLabel->setText(tr("PDF 第 %1 / %2 页").arg(page + 1).arg(total));
    });
    const int index = m_docTabs->addTab(tab, tab->displayName());
    m_docTabs->setCurrentIndex(index);
    updatePdfTabTitle(tab);
    updateUiForCurrentTab();
    statusBar()->showMessage(tr("已打开 PDF（只读）：%1").arg(filePath), 4000);
    return tab;
}

void MainWindow::bindTab(DocumentTab *tab)
{
    connect(tab, &DocumentTab::modificationChanged, this, &MainWindow::onDocumentModified);
    connect(tab, &DocumentTab::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(tab, &DocumentTab::contentsChanged, this, &MainWindow::onContentsChanged);
    connect(tab, &DocumentTab::pageInfoChanged, this, &MainWindow::onCursorMoved);
    connect(tab, &DocumentTab::zoomChanged, this, &MainWindow::updateZoomUi);
    connect(tab, &DocumentTab::editFormulaRequested, this, &MainWindow::editFormula);
    connect(tab->editor(), &PagedEditorWidget::currentCharFormatChanged,
            this, &MainWindow::currentCharFormatChanged);
    connect(tab->editor(), &PagedEditorWidget::copyAvailable, m_actionCut, &QAction::setEnabled);
    connect(tab->editor(), &PagedEditorWidget::copyAvailable, m_actionCopy, &QAction::setEnabled);
    connect(tab->editor(), &PagedEditorWidget::undoAvailable, m_actionUndo, &QAction::setEnabled);
    connect(tab->editor(), &PagedEditorWidget::redoAvailable, m_actionRedo, &QAction::setEnabled);
    connect(tab->editor(), &PagedEditorWidget::selectionChanged, this, [this]() {
        if (m_formatPainterArmed && currentEditor()
            && currentEditor()->textCursor().hasSelection())
            applyPickedFormat();
    });
    tab->setTableContextActions({
        m_actionTableAddRowAbove, m_actionTableAddRow,
        m_actionTableAddColLeft, m_actionTableAddCol,
        m_actionTableDelRow, m_actionTableDelCol, m_actionTableDelete,
        m_actionTableMerge, m_actionTableSplit,
        m_actionTableSelectRow, m_actionTableSelectCol, m_actionTableSelect,
        m_actionTableHeader, m_actionTableClearHeader, m_actionTableCellBg,
        m_actionTableColumnWidths, m_actionTableRowHeight,
        m_actionTableEvenCols, m_actionTableBandRows, m_actionTableProperties
    });
}

void MainWindow::updateTabTitle(DocumentTab *tab)
{
    if (!tab)
        return;
    const int index = m_docTabs->indexOf(tab);
    if (index < 0)
        return;
    QString title = tab->displayName();
    if (tab->isModified())
        title += QLatin1Char('*');
    m_docTabs->setTabText(index, title);
    if (tab == currentTab())
        updateWindowTitle();
}

void MainWindow::updatePdfTabTitle(PdfViewTab *tab)
{
    if (!tab)
        return;
    const int index = m_docTabs->indexOf(tab);
    if (index < 0)
        return;
    m_docTabs->setTabText(index, tab->displayName());
    if (tab == currentPdfTab())
        updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    if (PdfViewTab *pdf = currentPdfTab()) {
        setWindowTitle(tr("%1 — NewWord（PDF 阅读）").arg(pdf->displayName()));
        setWindowModified(false);
        return;
    }
    DocumentTab *tab = currentTab();
    const QString name = tab ? tab->displayName() : tr("未命名");
    setWindowTitle(tr("%1[*] — NewWord").arg(name));
    setWindowModified(tab && tab->isModified());
}

void MainWindow::updateUiForCurrentTab()
{
    const bool isDoc = currentTab() != nullptr;

    const QList<QAction *> docOnly = {
        m_actionSave, m_actionSaveAs, m_actionExportOdt, m_actionExportPdf,
        m_actionPrint, m_actionPreview, m_actionHeaderFooter, m_actionPageSetup,
        m_actionParagraph, m_actionUndo, m_actionRedo,
        m_actionCut, m_actionCopy, m_actionPaste, m_actionPastePlain,
        m_actionFind, m_actionBold, m_actionItalic, m_actionUnderline,
        m_actionStrike, m_actionSuper, m_actionSub, m_actionTextColor,
        m_actionHighlight, m_actionClearFormat, m_actionFontInc, m_actionFontDec,
        m_actionAlignLeft, m_actionAlignCenter, m_actionAlignRight, m_actionAlignJustify,
        m_actionBullet, m_actionNumbered, m_actionIndent, m_actionOutdent,
        m_actionNormal, m_actionTitleStyle, m_actionHeading1, m_actionHeading2,
        m_actionHeading3, m_actionHeading4, m_actionQuoteStyle,
        m_actionInsertImage, m_actionInsertFormula, m_actionInsertTable, m_actionInsertTextBox,
        m_actionInsertLink, m_actionInsertToc, m_actionInsertLine,
        m_actionInsertPageBreak, m_actionInsertSectionBreak, m_actionInsertFootnote,
        m_actionInsertEndnote,
        m_actionInsertComment, m_actionInsertDate, m_actionInsertTime,
        m_actionFormatPainter, m_actionSpellCheck, m_actionViewPage, m_actionViewDraft,
        m_actionViewOutline, m_actionViewReading, m_actionViewWeb, m_actionOutline,
        m_actionShowRuler, m_actionShowGrid
    };
    for (QAction *a : docOnly) {
        if (a)
            a->setEnabled(isDoc);
    }
    if (m_fontCombo)
        m_fontCombo->setEnabled(isDoc);
    if (m_sizeCombo)
        m_sizeCombo->setEnabled(isDoc);
    if (m_lineSpacingCombo)
        m_lineSpacingCombo->setEnabled(isDoc);
    if (m_outlinePane)
        m_outlinePane->setVisible(isDoc && m_actionOutline && m_actionOutline->isChecked());

    if (PdfViewTab *pdf = currentPdfTab()) {
        updateZoomUi(pdf->zoomPercent());
        if (m_wordCountLabel)
            m_wordCountLabel->setText(tr("PDF 只读"));
        if (m_positionLabel)
            m_positionLabel->setText(tr("PDF 第 %1 / %2 页")
                                        .arg(pdf->currentPage() + 1)
                                        .arg(pdf->pageCount()));
    } else if (currentTab()) {
        onCursorMoved();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Flush drafts before the save/discard dialog so a crash during quit still has a snapshot.
    if (m_autoSaveEnabled) {
        for (int i = 0; i < m_docTabs->count(); ++i) {
            if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i))) {
                if (tab->isModified())
                    writeTabDraft(tab);
            }
        }
    }

    if (maybeSaveAll()) {
        for (int i = 0; i < m_docTabs->count(); ++i) {
            if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i)))
                clearTabDraft(tab);
        }
        saveSessionFiles();
        saveRecentFiles();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_deferredStartupDone)
        return;
    m_deferredStartupDone = true;
    // Let the first frame paint before modal recovery / session file I/O.
    QTimer::singleShot(0, this, &MainWindow::finishDeferredStartup);
}

void MainWindow::finishDeferredStartup()
{
    promptRecoverDrafts();
    restoreSessionFiles();
}

void MainWindow::onTabChanged(int)
{
    m_cachedStatsRevision = -1;
    updateWindowTitle();
    updateUiForCurrentTab();

    if (PdfViewTab *pdf = currentPdfTab()) {
        updateZoomUi(pdf->zoomPercent());
        if (m_findDialog) {
            m_findDialog->deleteLater();
            m_findDialog = nullptr;
        }
        return;
    }

    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    updateZoomUi(tab->zoomPercent());
    onCursorMoved();
    onContentsChanged();
    currentCharFormatChanged(tab->editor()->currentCharFormat());
    m_outlinePane->setDocument(tab->editor()->document());
    syncViewModeActions();
    if (m_findDialog) {
        m_findDialog->deleteLater();
        m_findDialog = nullptr;
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *widget = m_docTabs->widget(index);
    if (auto *tab = qobject_cast<DocumentTab *>(widget)) {
        if (!maybeSave(tab))
            return;
        clearTabDraft(tab);
        m_docTabs->removeTab(index);
        tab->deleteLater();
    } else if (auto *pdf = qobject_cast<PdfViewTab *>(widget)) {
        m_docTabs->removeTab(index);
        pdf->deleteLater();
    } else {
        return;
    }

    if (m_docTabs->count() == 0)
        createTab();
}
