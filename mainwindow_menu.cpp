#include "mainwindow.h"
#include "appstyle.h"
#include "lazyfontcombobox.h"
#include "pagededitorwidget.h"
#include "ribbonbar.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFont>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QTextEdit>
#include <QWidget>

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(m_actionNew);
    fileMenu->addAction(m_actionOpen);
    m_recentMenu = fileMenu->addMenu(tr("最近打开"));
    fileMenu->addAction(m_actionSave);
    fileMenu->addAction(m_actionSaveAs);
    fileMenu->addAction(m_actionCloseTab);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionExportPdf);
    fileMenu->addAction(m_actionExportOdt);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionPageSetup);
    fileMenu->addAction(m_actionHeaderFooter);
    fileMenu->addAction(m_actionPreview);
    fileMenu->addAction(m_actionPrint);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionExit);

    auto *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    editMenu->addAction(m_actionUndo);
    editMenu->addAction(m_actionRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actionCut);
    editMenu->addAction(m_actionCopy);
    editMenu->addAction(m_actionPaste);
    editMenu->addAction(m_actionPastePlain);
    editMenu->addSeparator();
    editMenu->addAction(m_actionFind);
    editMenu->addAction(m_actionSelectAll);
    editMenu->addSeparator();
    editMenu->addAction(m_actionSpellCheck);
    editMenu->addAction(m_actionStats);

    auto *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    auto *docViewMenu = viewMenu->addMenu(tr("文档视图"));
    docViewMenu->addAction(m_actionViewPage);
    docViewMenu->addAction(m_actionViewDraft);
    docViewMenu->addAction(m_actionViewOutline);
    docViewMenu->addAction(m_actionViewReading);
    docViewMenu->addAction(m_actionViewWeb);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionPagedEditorPrototype);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionOutline);
    viewMenu->addAction(m_actionShowRuler);
    viewMenu->addAction(m_actionShowGrid);
    viewMenu->addAction(m_actionShowRibbon);
    viewMenu->addAction(m_actionCollapseRibbon);
    viewMenu->addAction(m_actionZoomIn);
    viewMenu->addAction(m_actionZoomOut);
    viewMenu->addAction(m_actionZoomReset);
    viewMenu->addAction(m_actionPreview);
    viewMenu->addSeparator();
    auto *themeMenu = viewMenu->addMenu(tr("界面主题"));
    for (QAction *action : m_themeGroup->actions())
        themeMenu->addAction(action);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionAutoSave);

    auto *formatMenu = menuBar()->addMenu(tr("格式(&O)"));
    formatMenu->addAction(m_actionFormatPainter);
    formatMenu->addAction(m_actionBold);
    formatMenu->addAction(m_actionItalic);
    formatMenu->addAction(m_actionUnderline);
    formatMenu->addAction(m_actionStrike);
    formatMenu->addAction(m_actionSuper);
    formatMenu->addAction(m_actionSub);
    formatMenu->addSeparator();
    formatMenu->addAction(m_actionClearFormat);
    formatMenu->addAction(m_actionParagraph);
    auto *caseMenu = formatMenu->addMenu(tr("大小写"));
    caseMenu->addAction(m_actionUpper);
    caseMenu->addAction(m_actionLower);
    caseMenu->addAction(m_actionTitleCase);

    auto *insertMenu = menuBar()->addMenu(tr("插入(&I)"));
    insertMenu->addAction(m_actionInsertImage);
    insertMenu->addAction(m_actionImageProperties);
    insertMenu->addAction(m_actionInsertFormula);
    insertMenu->addAction(m_actionInsertTable);
    insertMenu->addAction(m_actionInsertTextBox);
    insertMenu->addAction(m_actionInsertLink);
    insertMenu->addAction(m_actionInsertToc);
    insertMenu->addAction(m_actionInsertPageBreak);
    insertMenu->addAction(m_actionInsertSectionBreak);
    insertMenu->addSeparator();
    insertMenu->addAction(m_actionInsertFootnote);
    insertMenu->addAction(m_actionInsertEndnote);
    insertMenu->addAction(m_actionInsertComment);
    insertMenu->addAction(m_actionShowComments);

    auto *tableMenu = menuBar()->addMenu(tr("表格(&T)"));
    tableMenu->addAction(m_actionInsertTable);
    tableMenu->addSeparator();
    auto *insertRowsMenu = tableMenu->addMenu(tr("插入"));
    insertRowsMenu->addAction(m_actionTableAddRowAbove);
    insertRowsMenu->addAction(m_actionTableAddRow);
    insertRowsMenu->addAction(m_actionTableAddColLeft);
    insertRowsMenu->addAction(m_actionTableAddCol);
    auto *deleteMenu = tableMenu->addMenu(tr("删除"));
    deleteMenu->addAction(m_actionTableDelRow);
    deleteMenu->addAction(m_actionTableDelCol);
    deleteMenu->addAction(m_actionTableDelete);
    tableMenu->addSeparator();
    tableMenu->addAction(m_actionTableMerge);
    tableMenu->addAction(m_actionTableSplit);
    tableMenu->addSeparator();
    auto *selectMenu = tableMenu->addMenu(tr("选择"));
    selectMenu->addAction(m_actionTableSelectRow);
    selectMenu->addAction(m_actionTableSelectCol);
    selectMenu->addAction(m_actionTableSelect);
    tableMenu->addSeparator();
    tableMenu->addAction(m_actionTableHeader);
    tableMenu->addAction(m_actionTableClearHeader);
    tableMenu->addAction(m_actionTableCellBg);
    tableMenu->addAction(m_actionTableColumnWidths);
    tableMenu->addAction(m_actionTableRowHeight);
    tableMenu->addAction(m_actionTableEvenCols);
    tableMenu->addAction(m_actionTableBandRows);
    tableMenu->addSeparator();
    tableMenu->addAction(m_actionTableProperties);

    auto *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(m_actionShortcuts);
    helpMenu->addAction(m_actionAbout);
}

void MainWindow::setupRibbon()
{
    m_ribbon->addHomeActions(
        {m_actionPaste, m_actionCut, m_actionCopy, m_actionFormatPainter},
        {m_actionBold, m_actionItalic, m_actionUnderline, m_actionStrike,
         m_actionSuper, m_actionSub, m_actionTextColor, m_actionHighlight,
         m_actionClearFormat, m_actionFontInc, m_actionFontDec},
        m_fontCombo, m_sizeCombo, m_lineSpacingCombo,
        {m_actionAlignLeft, m_actionAlignCenter, m_actionAlignRight, m_actionAlignJustify,
         m_actionBullet, m_actionNumbered, m_actionOutdent, m_actionIndent},
        {m_actionNormal, m_actionTitleStyle, m_actionHeading1, m_actionHeading2,
         m_actionHeading3, m_actionHeading4, m_actionQuoteStyle});

    m_ribbon->addInsertActions(
        {m_actionInsertImage, m_actionImageProperties, m_actionInsertFormula, m_actionInsertTable,
         m_actionInsertTextBox, m_actionInsertLink,
         m_actionInsertToc, m_actionInsertLine, m_actionInsertPageBreak, m_actionInsertSectionBreak,
         m_actionInsertFootnote, m_actionInsertEndnote, m_actionInsertComment, m_actionInsertDate, m_actionInsertTime},
        {m_actionTableAddRow, m_actionTableAddCol, m_actionTableDelRow, m_actionTableDelCol,
         m_actionTableMerge, m_actionTableSplit, m_actionTableHeader, m_actionTableCellBg,
         m_actionTableProperties});

    m_ribbon->addLayoutActions(
        {m_actionPageSetup, m_actionParagraph, m_actionHeaderFooter,
         m_actionPreview, m_actionPrint, m_actionExportPdf});

    m_ribbon->addViewActions(
        {m_actionViewPage, m_actionViewDraft, m_actionViewOutline,
         m_actionViewReading, m_actionViewWeb},
        {m_actionOutline, m_actionShowRuler, m_actionShowGrid,
         m_actionShowRibbon, m_actionCollapseRibbon,
         m_actionZoomIn, m_actionZoomOut, m_actionZoomReset,
         m_actionSpellCheck, m_actionAutoSave, m_actionPreview},
        m_themeGroup->actions());
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(tr("字数: 0"), this);
    m_positionLabel = new QLabel(tr("行 1, 列 1"), this);
    m_zoomLabel = new QLabel(tr("100%"), this);
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(50, 200);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setSingleStep(10);
    m_zoomSlider->setPageStep(25);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    m_zoomSlider->setTickInterval(25);
    m_zoomSlider->setFixedWidth(120);

    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_wordCountLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(m_zoomSlider);
    statusBar()->setStyleSheet(AppStyle::statusBarStyleSheet());
    statusBar()->showMessage(tr("就绪"));
}

void MainWindow::setupConnections()
{
    connect(m_actionNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(m_actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(m_actionCloseTab, &QAction::triggered, this, &MainWindow::closeCurrentTab);
    connect(m_actionExportOdt, &QAction::triggered, this, &MainWindow::exportOdt);
    connect(m_actionExportPdf, &QAction::triggered, this, &MainWindow::exportPdf);
    connect(m_actionPrint, &QAction::triggered, this, &MainWindow::printDocument);
    connect(m_actionPreview, &QAction::triggered, this, &MainWindow::pagePreview);
    connect(m_actionHeaderFooter, &QAction::triggered, this, &MainWindow::editHeaderFooter);
    connect(m_actionPageSetup, &QAction::triggered, this, &MainWindow::pageSetup);
    connect(m_actionParagraph, &QAction::triggered, this, &MainWindow::editParagraph);
    connect(m_actionExit, &QAction::triggered, this, &QWidget::close);
    connect(m_actionClearRecent, &QAction::triggered, this, &MainWindow::clearRecentFiles);

    connect(m_actionUndo, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->undo();
    });
    connect(m_actionRedo, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->redo();
    });
    connect(m_actionCut, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->cut();
    });
    connect(m_actionCopy, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->copy();
    });
    connect(m_actionPaste, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->paste();
    });
    connect(m_actionPastePlain, &QAction::triggered, this, &MainWindow::pastePlainText);
    connect(m_actionFind, &QAction::triggered, this, &MainWindow::showFindReplace);
    connect(m_actionSelectAll, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->selectAll();
    });
    connect(m_actionStats, &QAction::triggered, this, &MainWindow::showDocumentStats);
    connect(m_actionSpellCheck, &QAction::toggled, this, &MainWindow::toggleSpellCheck);

    connect(m_actionBold, &QAction::triggered, this, &MainWindow::textBold);
    connect(m_actionItalic, &QAction::triggered, this, &MainWindow::textItalic);
    connect(m_actionUnderline, &QAction::triggered, this, &MainWindow::textUnderline);
    connect(m_actionStrike, &QAction::triggered, this, &MainWindow::textStrikeout);
    connect(m_actionSuper, &QAction::triggered, this, &MainWindow::textSuperScript);
    connect(m_actionSub, &QAction::triggered, this, &MainWindow::textSubScript);
    connect(m_actionTextColor, &QAction::triggered, this, &MainWindow::textColor);
    connect(m_actionHighlight, &QAction::triggered, this, &MainWindow::textHighlight);
    connect(m_actionClearFormat, &QAction::triggered, this, &MainWindow::clearFormatting);
    connect(m_actionFontInc, &QAction::triggered, this, &MainWindow::increaseFontSize);
    connect(m_actionFontDec, &QAction::triggered, this, &MainWindow::decreaseFontSize);
    connect(m_actionUpper, &QAction::triggered, this, &MainWindow::toUpperCase);
    connect(m_actionLower, &QAction::triggered, this, &MainWindow::toLowerCase);
    connect(m_actionTitleCase, &QAction::triggered, this, &MainWindow::toTitleCase);

    connect(m_actionAlignLeft, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    });
    connect(m_actionAlignCenter, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->setAlignment(Qt::AlignHCenter);
    });
    connect(m_actionAlignRight, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    });
    connect(m_actionAlignJustify, &QAction::triggered, this, [this]() {
        if (auto *e = currentEditor()) e->setAlignment(Qt::AlignJustify);
    });

    connect(m_actionBullet, &QAction::triggered, this, &MainWindow::toggleBulletList);
    connect(m_actionNumbered, &QAction::triggered, this, &MainWindow::toggleNumberedList);
    connect(m_actionIndent, &QAction::triggered, this, &MainWindow::increaseIndent);
    connect(m_actionOutdent, &QAction::triggered, this, &MainWindow::decreaseIndent);
    connect(m_actionHeading1, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionHeading2, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionHeading3, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionHeading4, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionTitleStyle, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionQuoteStyle, &QAction::triggered, this, &MainWindow::applyDocumentStyle);
    connect(m_actionNormal, &QAction::triggered, this, &MainWindow::applyDocumentStyle);

    connect(m_actionInsertImage, &QAction::triggered, this, &MainWindow::insertImage);
    connect(m_actionImageProperties, &QAction::triggered, this, &MainWindow::editImageProperties);
    connect(m_actionInsertFormula, &QAction::triggered, this, &MainWindow::insertFormula);
    connect(m_actionInsertTable, &QAction::triggered, this, &MainWindow::insertTable);
    connect(m_actionInsertTextBox, &QAction::triggered, this, &MainWindow::insertTextBox);
    connect(m_actionTableAddRow, &QAction::triggered, this, &MainWindow::insertTableRow);
    connect(m_actionTableAddRowAbove, &QAction::triggered, this, &MainWindow::insertTableRowAbove);
    connect(m_actionTableAddCol, &QAction::triggered, this, &MainWindow::insertTableColumn);
    connect(m_actionTableAddColLeft, &QAction::triggered, this, &MainWindow::insertTableColumnLeft);
    connect(m_actionTableDelRow, &QAction::triggered, this, &MainWindow::removeTableRow);
    connect(m_actionTableDelCol, &QAction::triggered, this, &MainWindow::removeTableColumn);
    connect(m_actionTableDelete, &QAction::triggered, this, &MainWindow::deleteTable);
    connect(m_actionTableMerge, &QAction::triggered, this, &MainWindow::mergeTableCells);
    connect(m_actionTableSplit, &QAction::triggered, this, &MainWindow::splitTableCells);
    connect(m_actionTableHeader, &QAction::triggered, this, &MainWindow::styleTableHeader);
    connect(m_actionTableClearHeader, &QAction::triggered, this, &MainWindow::clearTableHeader);
    connect(m_actionTableSelectRow, &QAction::triggered, this, &MainWindow::selectTableRow);
    connect(m_actionTableSelectCol, &QAction::triggered, this, &MainWindow::selectTableColumn);
    connect(m_actionTableSelect, &QAction::triggered, this, &MainWindow::selectTable);
    connect(m_actionTableCellBg, &QAction::triggered, this, &MainWindow::setTableCellBackground);
    connect(m_actionTableEvenCols, &QAction::triggered, this, &MainWindow::evenTableColumns);
    connect(m_actionTableColumnWidths, &QAction::triggered, this, &MainWindow::editTableColumnWidths);
    connect(m_actionTableRowHeight, &QAction::triggered, this, &MainWindow::editTableRowHeight);
    connect(m_actionTableBandRows, &QAction::triggered, this, &MainWindow::bandTableRows);
    connect(m_actionTableProperties, &QAction::triggered, this, &MainWindow::tableProperties);
    connect(m_actionInsertLine, &QAction::triggered, this, &MainWindow::insertHorizontalLine);
    connect(m_actionInsertPageBreak, &QAction::triggered, this, &MainWindow::insertPageBreak);
    connect(m_actionInsertSectionBreak, &QAction::triggered, this, &MainWindow::insertSectionBreak);
    connect(m_actionInsertFootnote, &QAction::triggered, this, &MainWindow::insertFootnote);
    connect(m_actionInsertEndnote, &QAction::triggered, this, &MainWindow::insertEndnote);
    connect(m_actionInsertComment, &QAction::triggered, this, &MainWindow::insertComment);
    connect(m_actionShowComments, &QAction::triggered, this, &MainWindow::showComments);
    connect(m_actionInsertDate, &QAction::triggered, this, &MainWindow::insertDate);
    connect(m_actionInsertTime, &QAction::triggered, this, &MainWindow::insertTime);
    connect(m_actionInsertLink, &QAction::triggered, this, &MainWindow::insertLink);
    connect(m_actionInsertToc, &QAction::triggered, this, &MainWindow::insertTableOfContents);
    connect(m_actionFormatPainter, &QAction::triggered, this, &MainWindow::pickFormat);
    connect(m_actionOutline, &QAction::toggled, this, &MainWindow::toggleOutline);
    connect(m_actionShowRuler, &QAction::toggled, this, &MainWindow::toggleRuler);
    connect(m_actionShowGrid, &QAction::toggled, this, &MainWindow::toggleGridLines);
    connect(m_actionShowRibbon, &QAction::toggled, this, [this](bool on) {
        if (m_ribbon)
            m_ribbon->setVisible(on);
        if (m_actionCollapseRibbon)
            m_actionCollapseRibbon->setEnabled(on);
    });
    connect(m_actionCollapseRibbon, &QAction::toggled, this, [this](bool on) {
        if (m_ribbon)
            m_ribbon->setCollapsed(on);
    });
    connect(m_ribbon, &RibbonBar::collapsedChanged, this, [this](bool collapsed) {
        if (!m_actionCollapseRibbon)
            return;
        const QSignalBlocker blocker(m_actionCollapseRibbon);
        m_actionCollapseRibbon->setChecked(collapsed);
    });
    connect(m_actionAutoSave, &QAction::toggled, this, &MainWindow::toggleAutoSave);
    connect(m_actionShortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    connect(m_actionAbout, &QAction::triggered, this, &MainWindow::about);

    connect(m_actionZoomIn, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(m_actionZoomOut, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(m_actionZoomReset, &QAction::triggered, this, &MainWindow::zoomReset);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::setZoomPercent);
    connect(m_actionViewPage, &QAction::triggered, this, &MainWindow::setDocumentViewMode);
    connect(m_actionViewDraft, &QAction::triggered, this, &MainWindow::setDocumentViewMode);
    connect(m_actionViewOutline, &QAction::triggered, this, &MainWindow::setDocumentViewMode);
    connect(m_actionViewReading, &QAction::triggered, this, &MainWindow::setDocumentViewMode);
    connect(m_actionViewWeb, &QAction::triggered, this, &MainWindow::setDocumentViewMode);
    connect(m_actionPagedEditorPrototype, &QAction::triggered, this,
            &MainWindow::openPagedEditorPrototype);
    connect(m_themeGroup, &QActionGroup::triggered, this, &MainWindow::setUiTheme);

    connect(m_fontCombo, &LazyFontComboBox::currentFontChanged, this,
            [this](const QFont &font) { textFamily(font.family()); });
    connect(m_sizeCombo, &QComboBox::textActivated, this, &MainWindow::textSize);
    connect(m_lineSpacingCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setLineSpacing(m_lineSpacingCombo->itemData(index).toDouble());
    });

    connect(m_docTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_docTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_actionCut->setEnabled(false);
    m_actionCopy->setEnabled(false);
    m_actionUndo->setEnabled(false);
    m_actionRedo->setEnabled(false);
    updateTableActions();
}
