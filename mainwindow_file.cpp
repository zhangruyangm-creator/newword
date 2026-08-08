#include "mainwindow.h"
#include "documentrecovery.h"
#include "documenttab.h"
#include "docxconverter.h"
#include "headerfooterdialog.h"
#include "pagepreview.h"
#include "pagedocumentpainter.h"
#include "pagesetupdialog.h"
#include "paragraphdialog.h"
#include "pdfviewtab.h"
#include "spellchecker.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMenu>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QSettings>
#include <QStatusBar>
#include <QTextDocumentWriter>
#include <QTextEdit>
#include <QtConcurrent>

namespace {
constexpr int kMaxRecentFiles = 8;
} // namespace

void MainWindow::newFile()
{
    createTab();
}

void MainWindow::openFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("打开文档"), QString(),
        tr("Word 文档 (*.docx);;PDF 文件 (*.pdf);;Markdown (*.md *.markdown);;HTML 文档 (*.html *.htm);;文本文件 (*.txt);;所有文件 (*)"));
    if (!fileName.isEmpty())
        loadFile(fileName);
}

bool MainWindow::saveFile()
{
    if (currentPdfTab()) {
        statusBar()->showMessage(tr("PDF 为只读预览，无法在此保存。请使用导出功能从文档生成 PDF。"), 4000);
        return false;
    }
    DocumentTab *tab = currentTab();
    if (!tab)
        return false;
    if (tab->filePath().isEmpty())
        return saveFileAs();
    return saveTabToFile(tab, tab->filePath());
}

bool MainWindow::saveFileAs()
{
    if (currentPdfTab()) {
        statusBar()->showMessage(tr("PDF 为只读预览，无法另存为可编辑文档。"), 4000);
        return false;
    }
    DocumentTab *tab = currentTab();
    if (!tab)
        return false;
    QString suggested = QStringLiteral("未命名.docx");
    if (!tab->filePath().isEmpty())
        suggested = tab->filePath();
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("另存为"), suggested,
        tr("Word 文档 (*.docx);;Markdown (*.md *.markdown);;HTML 文档 (*.html *.htm);;文本文件 (*.txt);;ODT 文档 (*.odt)"));
    if (fileName.isEmpty())
        return false;
    return saveTabToFile(tab, fileName);
}

void MainWindow::closeCurrentTab()
{
    onTabCloseRequested(m_docTabs->currentIndex());
}

void MainWindow::exportOdt()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("导出 ODT"), QStringLiteral("未命名.odt"),
        tr("OpenDocument 文本 (*.odt)"));
    if (fileName.isEmpty())
        return;
    QTextDocumentWriter writer(fileName, "odf");
    if (!writer.write(tab->editor()->document())) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法导出文件。"));
        return;
    }
    statusBar()->showMessage(tr("已导出 %1").arg(fileName), 3000);
}

void MainWindow::exportPdf()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("导出 PDF"), QStringLiteral("未命名.pdf"), tr("PDF 文件 (*.pdf)"));
    if (fileName.isEmpty())
        return;

    const PageLayoutSettings layout = tab->pageLayout();
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageOrientation(layout.orientation == PageLayoutSettings::Orientation::Landscape
                                   ? QPageLayout::Landscape
                                   : QPageLayout::Portrait);
    if (layout.paper == PageLayoutSettings::Paper::Custom) {
        printer.setPageSize(QPageSize(layout.pageSizeMm(), QPageSize::Millimeter));
    } else if (layout.paper == PageLayoutSettings::Paper::A5) {
        printer.setPageSize(QPageSize(QPageSize::A5));
    } else if (layout.paper == PageLayoutSettings::Paper::Letter) {
        printer.setPageSize(QPageSize(QPageSize::Letter));
    } else if (layout.paper == PageLayoutSettings::Paper::Legal) {
        printer.setPageSize(QPageSize(QPageSize::Legal));
    } else {
        printer.setPageSize(QPageSize(QPageSize::A4));
    }
    printer.setPageMargins(layout.marginsMm, QPageLayout::Millimeter);

    if (!PageDocumentPainter::printDocument(&printer, tab->editor()->document(), layout,
                                            tab->headerFooter())) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法导出 PDF。"));
        return;
    }
    statusBar()->showMessage(tr("已导出 PDF：%1").arg(fileName), 3000);
}

void MainWindow::printDocument()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;

    const PageLayoutSettings layout = tab->pageLayout();
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(layout.orientation == PageLayoutSettings::Orientation::Landscape
                                   ? QPageLayout::Landscape
                                   : QPageLayout::Portrait);
    if (layout.paper == PageLayoutSettings::Paper::Custom) {
        printer.setPageSize(QPageSize(layout.pageSizeMm(), QPageSize::Millimeter));
    } else if (layout.paper == PageLayoutSettings::Paper::A5) {
        printer.setPageSize(QPageSize(QPageSize::A5));
    } else if (layout.paper == PageLayoutSettings::Paper::Letter) {
        printer.setPageSize(QPageSize(QPageSize::Letter));
    } else if (layout.paper == PageLayoutSettings::Paper::Legal) {
        printer.setPageSize(QPageSize(QPageSize::Legal));
    } else {
        printer.setPageSize(QPageSize(QPageSize::A4));
    }

    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!PageDocumentPainter::printDocument(&printer, tab->editor()->document(), layout,
                                            tab->headerFooter())) {
        QMessageBox::warning(this, tr("打印失败"), tr("无法打印文档。"));
    }
}

void MainWindow::pagePreview()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    PagePreviewDialog dialog(tab->editor()->document(), tab->headerFooter(),
                             tab->pageLayout(), this);
    dialog.exec();
}

void MainWindow::editHeaderFooter()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    HeaderFooterDialog dialog(tab->headerFooter(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    tab->setHeaderFooter(dialog.settings());
    statusBar()->showMessage(tr("页眉页脚已更新"), 2000);
}

void MainWindow::pageSetup()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;

    PageSetupDialog dialog(tab->pageLayout(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const PageLayoutSettings layout = dialog.settings();
    tab->setPageLayout(layout);

    const QSizeF size = layout.pageSizeMm();
    statusBar()->showMessage(
        tr("页面：%1 %2 · %3×%4 mm")
            .arg(layout.paperName())
            .arg(layout.orientation == PageLayoutSettings::Orientation::Landscape ? tr("横向")
                                                                                   : tr("纵向"))
            .arg(size.width(), 0, 'f', 0)
            .arg(size.height(), 0, 'f', 0),
        3500);
}

void MainWindow::editParagraph()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    ParagraphDialog dialog(editor->textCursor().blockFormat(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        // Apply to all blocks in selection
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();
        cursor.setPosition(start);
        while (cursor.position() <= end && !cursor.atEnd()) {
            cursor.mergeBlockFormat(dialog.format());
            if (!cursor.movePosition(QTextCursor::NextBlock))
                break;
            if (cursor.position() > end)
                break;
        }
    } else {
        cursor.mergeBlockFormat(dialog.format());
    }
    cursor.endEditBlock();
    statusBar()->showMessage(tr("段落格式已应用"), 2000);
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    const QString fileName = action->data().toString();
    if (fileName.isEmpty())
        return;
    if (!QFileInfo::exists(fileName)) {
        QMessageBox::warning(this, tr("打开失败"), tr("文件不存在：%1").arg(fileName));
        m_recentFiles.removeAll(fileName);
        updateRecentFilesMenu();
        return;
    }
    loadFile(fileName);
}

void MainWindow::clearRecentFiles()
{
    m_recentFiles.clear();
    updateRecentFilesMenu();
    saveRecentFiles();
}

void MainWindow::loadRecentFiles()
{
    QSettings settings;
    m_recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
    m_spellCheckEnabled = settings.value(QStringLiteral("spellCheck"), true).toBool();
    m_autoSaveEnabled = settings.value(QStringLiteral("autoSave"), true).toBool();
    m_actionSpellCheck->setChecked(m_spellCheckEnabled && SpellChecker::isAvailable());
    m_actionAutoSave->setChecked(m_autoSaveEnabled);
}

void MainWindow::saveRecentFiles()
{
    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
    settings.setValue(QStringLiteral("spellCheck"), m_spellCheckEnabled);
    settings.setValue(QStringLiteral("autoSave"), m_autoSaveEnabled);
}

void MainWindow::addToRecentFiles(const QString &fileName)
{
    m_recentFiles.removeAll(fileName);
    m_recentFiles.prepend(fileName);
    while (m_recentFiles.size() > kMaxRecentFiles)
        m_recentFiles.removeLast();
    updateRecentFilesMenu();
    saveRecentFiles();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    int index = 0;
    for (const QString &file : m_recentFiles) {
        auto *action = m_recentMenu->addAction(
            QStringLiteral("%1  %2").arg(++index).arg(QFileInfo(file).fileName()));
        action->setData(file);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
    if (m_recentFiles.isEmpty()) {
        auto *empty = m_recentMenu->addAction(tr("（空）"));
        empty->setEnabled(false);
    } else {
        m_recentMenu->addSeparator();
        m_recentMenu->addAction(m_actionClearRecent);
    }
}

bool MainWindow::maybeSave(DocumentTab *tab)
{
    if (!tab || !tab->isModified())
        return true;
    m_docTabs->setCurrentWidget(tab);
    const auto ret = QMessageBox::warning(
        this, tr("NewWord"),
        tr("文档“%1”已被修改。\n是否保存更改？").arg(tab->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)
        return saveFile();
    if (ret == QMessageBox::Cancel)
        return false;
    return true;
}

bool MainWindow::maybeSaveAll()
{
    for (int i = 0; i < m_docTabs->count(); ++i) {
        auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i));
        if (!maybeSave(tab))
            return false;
    }
    return true;
}

void MainWindow::loadFile(const QString &fileName)
{
    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i))) {
            if (tab->filePath() == fileName) {
                m_docTabs->setCurrentIndex(i);
                return;
            }
        } else if (auto *pdf = qobject_cast<PdfViewTab *>(m_docTabs->widget(i))) {
            if (pdf->filePath() == fileName) {
                m_docTabs->setCurrentIndex(i);
                return;
            }
        }
    }

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("pdf")) {
        if (createPdfTab(fileName))
            addToRecentFiles(fileName);
        return;
    }

    if (suffix == QLatin1String("docx")) {
        openDocxAsync(fileName);
        return;
    }

    if (createTab(fileName))
        addToRecentFiles(fileName);
}

DocumentTab *MainWindow::blankStarterTab() const
{
    if (!m_docTabs || m_docTabs->count() != 1)
        return nullptr;
    auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(0));
    if (!tab || tab->isModified() || !tab->filePath().isEmpty())
        return nullptr;
    if (!tab->editor()->toPlainText().trimmed().isEmpty())
        return nullptr;
    return tab;
}

void MainWindow::openDocxAsync(const QString &filePath)
{
    if (m_docxOpenBusy) {
        if (!m_pendingDocxOpens.contains(filePath))
            m_pendingDocxOpens.append(filePath);
        statusBar()->showMessage(tr("已加入打开队列：%1").arg(QFileInfo(filePath).fileName()), 3000);
        return;
    }

    m_docxOpenBusy = true;
    m_docxOpenCancel = std::make_shared<std::atomic_bool>(false);

    if (m_docxProgress) {
        m_docxProgress->deleteLater();
        m_docxProgress = nullptr;
    }
    m_docxProgress = new QProgressDialog(tr("正在打开 DOCX…\n%1").arg(QFileInfo(filePath).fileName()),
                                         tr("取消"), 0, 0, this);
    m_docxProgress->setWindowModality(Qt::WindowModal);
    m_docxProgress->setMinimumDuration(150);
    m_docxProgress->setAutoClose(false);
    m_docxProgress->setAutoReset(false);
    connect(m_docxProgress, &QProgressDialog::canceled, this, [this]() {
        if (m_docxOpenCancel)
            m_docxOpenCancel->store(true);
    });
    m_docxProgress->show();
    statusBar()->showMessage(tr("正在转换 DOCX（后台）…"));

    auto cancel = m_docxOpenCancel;
    auto *watcher = new QFutureWatcher<DocxConverter::PrepareResult>(this);
    connect(watcher, &QFutureWatcher<DocxConverter::PrepareResult>::finished, this,
            [this, watcher, filePath]() {
                const DocxConverter::PrepareResult prepared = watcher->result();
                watcher->deleteLater();
                finishDocxOpen(filePath, prepared);
            });
    watcher->setFuture(QtConcurrent::run([filePath, cancel]() {
        return DocxConverter::prepareImport(filePath, cancel.get());
    }));
}

void MainWindow::finishDocxOpen(const QString &filePath, const DocxConverter::PrepareResult &prepared)
{
    if (m_docxProgress) {
        m_docxProgress->hide();
        m_docxProgress->deleteLater();
        m_docxProgress = nullptr;
    }

    const bool cancelled = prepared.cancelled
        || (m_docxOpenCancel && m_docxOpenCancel->load());
    m_docxOpenBusy = false;
    m_docxOpenCancel.reset();

    if (cancelled) {
        statusBar()->showMessage(tr("已取消打开"), 2500);
        pumpPendingDocxOpens();
        return;
    }

    DocumentTab *tab = blankStarterTab();
    const bool reusedBlank = tab != nullptr;
    if (!tab) {
        tab = new DocumentTab(m_docTabs);
        tab->setSpellCheckEnabled(m_spellCheckEnabled && SpellChecker::isAvailable());
    }

    QString error;
    bool ok = false;
    if (prepared.ok || prepared.useBuiltinOnGui) {
        statusBar()->showMessage(prepared.ok ? tr("正在载入文档…") : tr("正在用内置引擎打开…"));
        ok = tab->loadFromPreparedDocx(prepared, filePath, &error);
    } else {
        error = prepared.error.isEmpty() ? tr("无法打开 DOCX。") : prepared.error;
    }

    if (!ok) {
        if (!reusedBlank)
            tab->deleteLater();
        QMessageBox::warning(this, tr("打开失败"), error);
        statusBar()->clearMessage();
        pumpPendingDocxOpens();
        return;
    }

    if (!reusedBlank) {
        bindTab(tab);
        const int index = m_docTabs->addTab(tab, tab->displayName());
        m_docTabs->setCurrentIndex(index);
    } else {
        updateTabTitle(tab);
        updateUiForCurrentTab();
    }

    addToRecentFiles(filePath);
    const QString note = prepared.ok && !prepared.statusNote.isEmpty()
        ? prepared.statusNote
        : DocxConverter::lastStatusNote();
    statusBar()->showMessage(note.isEmpty() ? tr("已打开 %1").arg(filePath) : note, 5000);
    pumpPendingDocxOpens();
}

void MainWindow::pumpPendingDocxOpens()
{
    if (m_docxOpenBusy || m_pendingDocxOpens.isEmpty())
        return;
    openDocxAsync(m_pendingDocxOpens.takeFirst());
}

bool MainWindow::saveTabToFile(DocumentTab *tab, const QString &fileName)
{
    QString error;
    if (!tab->saveToFile(fileName, &error)) {
        QMessageBox::warning(this, tr("保存失败"), error);
        return false;
    }
    clearTabDraft(tab);
    updateTabTitle(tab);
    addToRecentFiles(fileName);
    if (QFileInfo(fileName).suffix().compare(QLatin1String("docx"), Qt::CaseInsensitive) == 0) {
        const QString note = DocxConverter::lastStatusNote();
        statusBar()->showMessage(
            note.isEmpty() ? tr("已保存 %1").arg(fileName)
                           : tr("已保存 %1 — %2").arg(fileName, note),
            5000);
    } else {
        statusBar()->showMessage(tr("已保存 %1").arg(fileName), 3000);
    }
    return true;
}
