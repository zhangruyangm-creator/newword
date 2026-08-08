#include "mainwindow.h"
#include "appstyle.h"
#include "documentrecovery.h"
#include "documenttab.h"
#include "docxconverter.h"
#include "findreplacedialog.h"
#include "outlinepane.h"
#include "pagededitorwidget.h"
#include "pdfviewtab.h"
#include "ribbonbar.h"
#include "spellchecker.h"
#include "textstats.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QSlider>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

void MainWindow::openPagedEditorPrototype()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("真实分页编辑（原型）— %1").arg(tab->displayName()));
    dialog->resize(1024, 780);

    // The prototype edits a copy so the current QTextEdit-based tab is unaffected.
    auto *doc = new QTextDocument(dialog);
    doc->setUndoRedoEnabled(true);
    doc->setDefaultFont(tab->editor()->document()->defaultFont());
    doc->setHtml(tab->editor()->toHtml());

    auto *view = new PagedEditorWidget(doc, tab->pageLayout(), tab->headerFooter(), dialog);

    auto *hint = new QLabel(
        tr("真实分页原型：每一页是一张完整的纸，文字、页眉、页脚互不穿越；"
           "可以输入、选择、复制粘贴、撤销重做，支持中文输入法。"
           "此窗口编辑的是当前文档的副本，关闭不影响原文档。"),
        dialog);
    hint->setWordWrap(true);

    auto *pageLabel = new QLabel(dialog);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(hint);
    layout->addWidget(view, 1);
    layout->addWidget(pageLabel);

    auto updatePageLabel = [view, pageLabel]() {
        pageLabel->setText(tr("第 %1 页 / 共 %2 页")
                               .arg(view->currentPageIndex() + 1)
                               .arg(view->pageCount()));
    };
    connect(view, &PagedEditorWidget::pageInfoChanged, dialog, updatePageLabel);
    updatePageLabel();

    dialog->show();
    view->setFocus();
}

void MainWindow::toggleOutline(bool visible)
{
    if (!m_outlinePane)
        return;
    m_outlinePane->setVisible(visible);
    if (m_bodySplitter && visible)
        m_bodySplitter->setSizes({220, qMax(400, width() - 220)});
}

void MainWindow::toggleRuler(bool visible)
{
    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i)))
            tab->setRulerVisible(visible);
    }
}

void MainWindow::toggleGridLines(bool visible)
{
    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i)))
            tab->setGridLinesVisible(visible);
    }
}

void MainWindow::toggleAutoSave(bool enabled)
{
    m_autoSaveEnabled = enabled;
    if (!m_autoSaveTimer)
        return;
    if (enabled)
        m_autoSaveTimer->start();
    else
        m_autoSaveTimer->stop();
    saveRecentFiles();
}

bool MainWindow::writeTabDraft(DocumentTab *tab, QString *errorMessage)
{
    if (!tab)
        return false;
    return DocumentRecovery::writeDraft(tab->recoveryId(),
                                        tab->filePath(),
                                        tab->displayName(),
                                        tab->editor()->document(),
                                        tab->headerFooter(),
                                        tab->pageLayout(),
                                        errorMessage);
}

void MainWindow::clearTabDraft(DocumentTab *tab)
{
    if (tab)
        DocumentRecovery::removeDraft(tab->recoveryId());
}

void MainWindow::autoSaveTick()
{
    int saved = 0;
    QString lastName;
    for (int i = 0; i < m_docTabs->count(); ++i) {
        auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i));
        if (!tab || !tab->isModified())
            continue;

        // Prefer writing the real file when the document already has a path.
        if (!tab->filePath().isEmpty()) {
            QString error;
            if (tab->saveToFile(tab->filePath(), &error)) {
                clearTabDraft(tab);
                updateTabTitle(tab);
                ++saved;
                lastName = QFileInfo(tab->filePath()).fileName();
                continue;
            }
        }

        QString error;
        if (writeTabDraft(tab, &error)) {
            ++saved;
            lastName = tab->displayName();
        }
    }

    if (saved > 0) {
        const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
        if (saved == 1) {
            statusBar()->showMessage(
                tr("已自动备份 %1 — %2").arg(time, lastName), 4000);
        } else {
            statusBar()->showMessage(
                tr("已自动备份 %1 — %2 个文档").arg(time).arg(saved), 4000);
        }
    }
}

void MainWindow::promptRecoverDrafts()
{
    const QVector<DocumentRecovery::Draft> drafts = DocumentRecovery::listDrafts();
    if (drafts.isEmpty())
        return;

    QStringList names;
    for (const DocumentRecovery::Draft &d : drafts) {
        const QString when = d.savedAt.isValid()
            ? d.savedAt.toString(QStringLiteral("MM-dd HH:mm"))
            : tr("未知时间");
        names << tr("• %1（%2）").arg(d.displayName, when);
    }

    const auto ret = QMessageBox::question(
        this, tr("恢复草稿"),
        tr("发现 %1 份未正常保存的草稿，是否恢复？\n\n%2")
            .arg(drafts.size())
            .arg(names.join(QLatin1Char('\n'))),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (ret != QMessageBox::Yes) {
        DocumentRecovery::removeAllDrafts();
        return;
    }

    // Drop the empty starter tab if it is still blank.
    if (m_docTabs->count() == 1) {
        if (auto *only = qobject_cast<DocumentTab *>(m_docTabs->widget(0))) {
            if (only->filePath().isEmpty() && !only->isModified()
                && only->editor()->toPlainText().trimmed().isEmpty()) {
                m_docTabs->removeTab(0);
                only->deleteLater();
            }
        }
    }

    for (const DocumentRecovery::Draft &d : drafts) {
        QString html;
        QString error;
        if (!DocumentRecovery::loadDraftHtml(d, &html, &error)) {
            QMessageBox::warning(this, tr("恢复失败"),
                                 tr("无法读取草稿“%1”：%2").arg(d.displayName, error));
            continue;
        }
        auto *tab = new DocumentTab(m_docTabs);
        tab->setSpellCheckEnabled(m_spellCheckEnabled && SpellChecker::isAvailable());
        tab->loadRecoveryContent(html, d.headerFooter, d.pageLayout, d.sourcePath);
        bindTab(tab);
        const int index = m_docTabs->addTab(tab, tab->displayName());
        m_docTabs->setCurrentIndex(index);
        updateTabTitle(tab);
        DocumentRecovery::removeDraft(d.id);
        writeTabDraft(tab); // keep a snapshot under the new recovery id
    }

    if (m_docTabs->count() == 0)
        createTab();
    updateUiForCurrentTab();
    statusBar()->showMessage(tr("已恢复草稿，请尽快另存为正式文件。"), 5000);
}

void MainWindow::restoreSessionFiles()
{
    QSettings settings;
    const QStringList files = settings.value(QStringLiteral("sessionFiles")).toStringList();
    if (files.isEmpty())
        return;

    // Only reopen if we still just have a blank starter (no recovered drafts).
    if (m_docTabs->count() != 1)
        return;
    auto *only = qobject_cast<DocumentTab *>(m_docTabs->widget(0));
    if (!only || only->isModified() || !only->filePath().isEmpty())
        return;
    if (!only->editor()->toPlainText().trimmed().isEmpty())
        return;

    bool opened = false;
    for (const QString &path : files) {
        if (!QFileInfo::exists(path))
            continue;
        loadFile(path);
        opened = true;
    }
    if (opened)
        statusBar()->showMessage(tr("已恢复上次打开的文档。"), 3000);
}

void MainWindow::saveSessionFiles()
{
    QStringList files;
    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i))) {
            if (!tab->filePath().isEmpty())
                files << tab->filePath();
        } else if (auto *pdf = qobject_cast<PdfViewTab *>(m_docTabs->widget(i))) {
            if (!pdf->filePath().isEmpty())
                files << pdf->filePath();
        }
    }
    QSettings settings;
    settings.setValue(QStringLiteral("sessionFiles"), files);
}

void MainWindow::navigateOutline(int position)
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor(editor->document());
    cursor.setPosition(position);
    editor->setTextCursor(cursor);
    editor->setFocus();
    editor->ensureCursorVisible();
}

void MainWindow::refreshOutlineSoon()
{
    if (!m_outlineRefreshTimer)
        return;
    auto *editor = currentEditor();
    const int chars = editor && editor->document() ? editor->document()->characterCount() : 0;
    m_outlineRefreshTimer->setInterval(chars > 50000 ? 700 : 500);
    m_outlineRefreshTimer->start();
}

void MainWindow::updateOutline()
{
    DocumentTab *tab = currentTab();
    if (tab && m_outlinePane)
        m_outlinePane->setDocument(tab->editor()->document());
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("快捷键一览"),
        tr("⌘N  新建标签　　⌘O  打开　　⌘S  保存　　⌘W  关闭标签\n"
           "⌘B/I/U  粗体/斜体/下划线\n"
           "⌘K  插入超链接　　⌘F  查找替换\n"
           "⌘⇧=  插入公式　　⌘⇧C  格式刷　　⌘⇧N  导航窗格\n"
           "⌘⇧S  页面设置　　⌘⇧M  段落\n"
           "⌘⇧P  分页预览　　⌘⇧E  导出 PDF\n"
           "Ctrl+F1  折叠/展开功能区　　双击选项卡亦可\n"
           "视图：页面 / 草稿 / 大纲 / 阅读 / Web 版式\n"
           "显示：标尺、网格线、导航窗格、功能区\n"
           "⌘+/-  缩放　　⌘0  实际大小"));
}

void MainWindow::setDocumentViewMode()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;

    DocumentViewMode mode = DocumentViewMode::Page;
    if (m_actionViewDraft->isChecked())
        mode = DocumentViewMode::Draft;
    else if (m_actionViewOutline->isChecked())
        mode = DocumentViewMode::Outline;
    else if (m_actionViewReading->isChecked())
        mode = DocumentViewMode::Reading;
    else if (m_actionViewWeb->isChecked())
        mode = DocumentViewMode::Web;

    tab->setViewMode(mode);

    QString tip;
    switch (mode) {
    case DocumentViewMode::Page:
        tip = tr("页面视图");
        break;
    case DocumentViewMode::Draft:
        tip = tr("草稿视图");
        break;
    case DocumentViewMode::Outline:
        tip = tr("大纲视图");
        break;
    case DocumentViewMode::Reading:
        tip = tr("阅读视图（只读）");
        break;
    case DocumentViewMode::Web:
        tip = tr("Web 版式视图");
        break;
    }
    statusBar()->showMessage(tip, 2500);
}

void MainWindow::syncViewModeActions()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;

    const DocumentViewMode mode = tab->viewMode();
    m_actionViewPage->blockSignals(true);
    m_actionViewDraft->blockSignals(true);
    m_actionViewOutline->blockSignals(true);
    m_actionViewReading->blockSignals(true);
    m_actionViewWeb->blockSignals(true);

    m_actionViewPage->setChecked(mode == DocumentViewMode::Page);
    m_actionViewDraft->setChecked(mode == DocumentViewMode::Draft);
    m_actionViewOutline->setChecked(mode == DocumentViewMode::Outline);
    m_actionViewReading->setChecked(mode == DocumentViewMode::Reading);
    m_actionViewWeb->setChecked(mode == DocumentViewMode::Web);

    m_actionViewPage->blockSignals(false);
    m_actionViewDraft->blockSignals(false);
    m_actionViewOutline->blockSignals(false);
    m_actionViewReading->blockSignals(false);
    m_actionViewWeb->blockSignals(false);
}

void MainWindow::zoomIn()
{
    if (PdfViewTab *pdf = currentPdfTab()) {
        pdf->zoomIn();
        return;
    }
    if (DocumentTab *tab = currentTab())
        tab->setZoomPercent(tab->zoomPercent() + 10);
}

void MainWindow::zoomOut()
{
    if (PdfViewTab *pdf = currentPdfTab()) {
        pdf->zoomOut();
        return;
    }
    if (DocumentTab *tab = currentTab())
        tab->setZoomPercent(tab->zoomPercent() - 10);
}

void MainWindow::zoomReset()
{
    if (PdfViewTab *pdf = currentPdfTab()) {
        pdf->zoomReset();
        return;
    }
    if (DocumentTab *tab = currentTab())
        tab->setZoomPercent(100);
}

void MainWindow::setZoomPercent(int percent)
{
    if (PdfViewTab *pdf = currentPdfTab()) {
        pdf->setZoomPercent(percent);
        return;
    }
    if (DocumentTab *tab = currentTab())
        tab->setZoomPercent(percent);
}

void MainWindow::updateZoomUi(int percent)
{
    if (m_zoomSlider->value() != percent) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(percent);
        m_zoomSlider->blockSignals(false);
    }
    m_zoomLabel->setText(tr("%1%").arg(percent));
}

void MainWindow::toggleSpellCheck(bool enabled)
{
    m_spellCheckEnabled = enabled;
    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i)))
            tab->setSpellCheckEnabled(enabled && SpellChecker::isAvailable());
    }
    saveRecentFiles();
}

void MainWindow::showFindReplace()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    if (m_findDialog)
        m_findDialog->deleteLater();
    m_findDialog = new FindReplaceDialog(editor, this);
    m_findDialog->show();
    m_findDialog->raise();
    m_findDialog->activateWindow();
}

void MainWindow::showDocumentStats()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    const TextStats::Counts doc = TextStats::analyze(editor->toPlainText());
    int paragraphs = 0;
    for (QTextBlock b = editor->document()->begin(); b.isValid(); b = b.next()) {
        if (!b.text().trimmed().isEmpty())
            ++paragraphs;
    }

    QString body = tr("全文\n"
                      "字数: %1\n"
                      "字符（不计空白）: %2\n"
                      "字符（计空白）: %3\n"
                      "标点: %4\n"
                      "汉字: %5\n"
                      "段落: %6")
                       .arg(doc.words)
                       .arg(doc.chars)
                       .arg(doc.charsWithSpaces)
                       .arg(doc.punctuation)
                       .arg(doc.cjkChars)
                       .arg(paragraphs);

    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        const TextStats::Counts sel = TextStats::analyze(cursor.selectedText());
        body += tr("\n\n选中内容\n"
                   "字数: %1\n"
                   "字符（不计空白）: %2\n"
                   "字符（计空白）: %3\n"
                   "标点: %4\n"
                   "汉字: %5")
                    .arg(sel.words)
                    .arg(sel.chars)
                    .arg(sel.charsWithSpaces)
                    .arg(sel.punctuation)
                    .arg(sel.cjkChars);
    }

    QMessageBox::information(this, tr("字数统计"), body);
}

void MainWindow::about()
{
    QMessageBox::about(
        this, tr("关于 NewWord"),
        tr("<h3>NewWord 0.5</h3>"
           "<p><b>macOS 轻量文字处理</b>：写得顺、存得住、能导出 PDF；"
           "DOCX 能开能改，但不承诺与 Microsoft Word 完全一致。</p>"
           "<p><b>编辑</b>基于 Qt 富文本；"
           "<b>分页预览与 PDF/打印</b>使用自研 LayoutEngine"
           "（正文 / 表 / 图分页）。"
           "活页视图的页数与页缝由引擎驱动；行内断行仍可能与 PDF 略有差别。</p>"
           "<p><b>DOCX</b>：%1</p>"
           "<p>%2</p>"
           "<p>阶段 2：内置导出为 DocumentModel → DocxExporter。</p>"
           "<p>支持本地草稿自动备份与启动恢复；新建文档使用默认字体/纸张设置。</p>"
           "<p>详见项目 README 中的「能做什么 / 布局引擎 / 已知限制」。</p>")
            .arg(DocxConverter::bridgeStatusText(),
                 DocxConverter::primaryPathDescription()));
}

void MainWindow::setUiTheme(QAction *action)
{
    if (!action)
        return;
    applyUiTheme(static_cast<AppStyle::ThemeId>(action->data().toInt()));
}

void MainWindow::applyUiTheme(AppStyle::ThemeId id)
{
    AppStyle::setTheme(id);
    AppStyle::saveThemeToSettings();
    qApp->setStyleSheet(AppStyle::applicationStyleSheet());

    if (m_ribbon)
        m_ribbon->refreshTheme();
    if (m_outlinePane)
        m_outlinePane->refreshTheme();
    if (m_docTabs)
        m_docTabs->setStyleSheet(AppStyle::documentTabsStyleSheet());
    statusBar()->setStyleSheet(AppStyle::statusBarStyleSheet());

    for (int i = 0; i < m_docTabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_docTabs->widget(i)))
            tab->refreshTheme();
    }

    for (QAction *action : m_themeGroup->actions()) {
        if (static_cast<AppStyle::ThemeId>(action->data().toInt()) == id)
            action->setChecked(true);
    }
}
