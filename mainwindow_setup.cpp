#include "mainwindow.h"
#include "appstyle.h"
#include "lazyfontcombobox.h"
#include "outlinepane.h"
#include "ribbonbar.h"
#include "spellchecker.h"
#include "styleutils.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFontDatabase>
#include <QIcon>
#include <QKeySequence>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupActions();
    setupUi();
    setupMenus();
    setupRibbon();
    setupStatusBar();
    setupConnections();
    loadRecentFiles();
    updateRecentFilesMenu();

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(60000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveTick);
    if (m_autoSaveEnabled)
        m_autoSaveTimer->start();

    m_outlineRefreshTimer = new QTimer(this);
    m_outlineRefreshTimer->setSingleShot(true);
    m_outlineRefreshTimer->setInterval(500);
    connect(m_outlineRefreshTimer, &QTimer::timeout, this, &MainWindow::updateOutline);

    m_statsRefreshTimer = new QTimer(this);
    m_statsRefreshTimer->setSingleShot(true);
    m_statsRefreshTimer->setInterval(250);
    connect(m_statsRefreshTimer, &QTimer::timeout, this, &MainWindow::updateDocumentStats);

    createTab();
    resize(1180, 860);
    setMinimumSize(800, 560);
    updateWindowTitle();
    // Light-office default: navigation pane off until the user opens it.
    if (m_outlinePane)
        m_outlinePane->setVisible(false);
    // Draft recovery + session restore run after first show (see showEvent).
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_ribbon = new RibbonBar(central);

    m_outlinePane = new OutlinePane(central);
    m_outlinePane->setFixedWidth(220);

    m_docTabs = new QTabWidget(central);
    m_docTabs->setTabsClosable(true);
    m_docTabs->setMovable(true);
    m_docTabs->setDocumentMode(true);
    m_docTabs->setStyleSheet(AppStyle::documentTabsStyleSheet());

    m_bodySplitter = new QSplitter(Qt::Horizontal, central);
    m_bodySplitter->setHandleWidth(2);
    m_bodySplitter->setChildrenCollapsible(false);
    m_bodySplitter->addWidget(m_outlinePane);
    m_bodySplitter->addWidget(m_docTabs);
    m_bodySplitter->setStretchFactor(0, 0);
    m_bodySplitter->setStretchFactor(1, 1);
    m_bodySplitter->setSizes({220, 960});

    layout->addWidget(m_ribbon);
    layout->addWidget(m_bodySplitter, 1);
    setCentralWidget(central);

    connect(m_outlinePane, &OutlinePane::navigateToPosition, this, &MainWindow::navigateOutline);
    connect(m_outlinePane, &OutlinePane::closeRequested, this, [this]() {
        if (m_actionOutline)
            m_actionOutline->setChecked(false);
        else
            toggleOutline(false);
    });
}

void MainWindow::setupActions()
{
    m_actionNew = new QAction(tr("新建"), this);
    m_actionNew->setShortcut(QKeySequence::New);
    m_actionNew->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew));

    m_actionOpen = new QAction(tr("打开..."), this);
    m_actionOpen->setShortcut(QKeySequence::Open);
    m_actionOpen->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));

    m_actionSave = new QAction(tr("保存"), this);
    m_actionSave->setShortcut(QKeySequence::Save);
    m_actionSave->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave));

    m_actionSaveAs = new QAction(tr("另存为..."), this);
    m_actionSaveAs->setShortcut(QKeySequence::SaveAs);
    m_actionSaveAs->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSaveAs));

    m_actionCloseTab = new QAction(tr("关闭标签页"), this);
    m_actionCloseTab->setShortcut(QKeySequence::Close);

    m_actionExportOdt = new QAction(tr("导出 ODT..."), this);
    m_actionExportPdf = new QAction(tr("导出 PDF..."), this);
    m_actionExportPdf->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    m_actionExportPdf->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave));

    m_actionPrint = new QAction(tr("打印..."), this);
    m_actionPrint->setShortcut(QKeySequence::Print);
    m_actionPrint->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentPrint));

    m_actionPreview = new QAction(tr("分页预览"), this);
    m_actionPreview->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    m_actionHeaderFooter = new QAction(tr("页眉页脚"), this);
    m_actionPageSetup = new QAction(tr("页面设置"), this);
    m_actionPageSetup->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_actionParagraph = new QAction(tr("段落..."), this);
    m_actionParagraph->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    m_actionExit = new QAction(tr("退出"), this);
    m_actionExit->setShortcut(QKeySequence::Quit);
    m_actionClearRecent = new QAction(tr("清除最近打开"), this);

    m_actionUndo = new QAction(tr("撤销"), this);
    m_actionUndo->setShortcut(QKeySequence::Undo);
    m_actionUndo->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditUndo));
    m_actionRedo = new QAction(tr("重做"), this);
    m_actionRedo->setShortcut(QKeySequence::Redo);
    m_actionRedo->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditRedo));
    m_actionCut = new QAction(tr("剪切"), this);
    m_actionCut->setShortcut(QKeySequence::Cut);
    m_actionCut->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditCut));
    m_actionCopy = new QAction(tr("复制"), this);
    m_actionCopy->setShortcut(QKeySequence::Copy);
    m_actionCopy->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy));
    m_actionPaste = new QAction(tr("粘贴"), this);
    m_actionPaste->setShortcut(QKeySequence::Paste);
    m_actionPaste->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditPaste));
    m_actionPastePlain = new QAction(tr("粘贴纯文本"), this);
    m_actionPastePlain->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    m_actionFind = new QAction(tr("查找替换"), this);
    m_actionFind->setShortcut(QKeySequence::Find);
    m_actionFind->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditFind));
    m_actionSelectAll = new QAction(tr("全选"), this);
    m_actionSelectAll->setShortcut(QKeySequence::SelectAll);
    m_actionStats = new QAction(tr("字数统计"), this);
    m_actionSpellCheck = new QAction(tr("拼写检查"), this);
    m_actionSpellCheck->setCheckable(true);
    m_actionSpellCheck->setChecked(SpellChecker::isAvailable());
    m_actionSpellCheck->setEnabled(SpellChecker::isAvailable());

    m_actionBold = new QAction(tr("粗体"), this);
    m_actionBold->setShortcut(QKeySequence::Bold);
    m_actionBold->setCheckable(true);
    m_actionBold->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatTextBold));
    m_actionItalic = new QAction(tr("斜体"), this);
    m_actionItalic->setShortcut(QKeySequence::Italic);
    m_actionItalic->setCheckable(true);
    m_actionItalic->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatTextItalic));
    m_actionUnderline = new QAction(tr("下划线"), this);
    m_actionUnderline->setShortcut(QKeySequence::Underline);
    m_actionUnderline->setCheckable(true);
    m_actionUnderline->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatTextUnderline));
    m_actionStrike = new QAction(tr("删除线"), this);
    m_actionStrike->setCheckable(true);
    m_actionSuper = new QAction(tr("上标"), this);
    m_actionSuper->setCheckable(true);
    m_actionSub = new QAction(tr("下标"), this);
    m_actionSub->setCheckable(true);
    m_actionTextColor = new QAction(tr("颜色"), this);
    m_actionHighlight = new QAction(tr("高亮"), this);
    m_actionClearFormat = new QAction(tr("清除格式"), this);
    m_actionClearFormat->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));

    m_actionAlignLeft = new QAction(tr("左对齐"), this);
    m_actionAlignLeft->setCheckable(true);
    m_actionAlignLeft->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatJustifyLeft));
    m_actionAlignCenter = new QAction(tr("居中"), this);
    m_actionAlignCenter->setCheckable(true);
    m_actionAlignCenter->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatJustifyCenter));
    m_actionAlignRight = new QAction(tr("右对齐"), this);
    m_actionAlignRight->setCheckable(true);
    m_actionAlignRight->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatJustifyRight));
    m_actionAlignJustify = new QAction(tr("两端对齐"), this);
    m_actionAlignJustify->setCheckable(true);
    m_actionAlignJustify->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::FormatJustifyFill));

    auto *alignGroup = new QActionGroup(this);
    alignGroup->addAction(m_actionAlignLeft);
    alignGroup->addAction(m_actionAlignCenter);
    alignGroup->addAction(m_actionAlignRight);
    alignGroup->addAction(m_actionAlignJustify);
    m_actionAlignLeft->setChecked(true);

    m_actionBullet = new QAction(tr("项目符号"), this);
    m_actionBullet->setCheckable(true);
    m_actionNumbered = new QAction(tr("编号"), this);
    m_actionNumbered->setCheckable(true);
    m_actionIndent = new QAction(tr("增加缩进"), this);
    m_actionOutdent = new QAction(tr("减少缩进"), this);
    m_actionFontInc = new QAction(tr("增大字号"), this);
    m_actionFontDec = new QAction(tr("减小字号"), this);

    m_actionHeading1 = new QAction(tr("标题 1"), this);
    m_actionHeading2 = new QAction(tr("标题 2"), this);
    m_actionHeading3 = new QAction(tr("标题 3"), this);
    m_actionHeading4 = new QAction(tr("标题 4"), this);
    m_actionTitleStyle = new QAction(tr("标题"), this);
    m_actionQuoteStyle = new QAction(tr("引用"), this);
    m_actionNormal = new QAction(tr("正文"), this);
    for (QAction *a : {m_actionNormal, m_actionTitleStyle, m_actionHeading1, m_actionHeading2,
                       m_actionHeading3, m_actionHeading4, m_actionQuoteStyle}) {
        a->setCheckable(true);
    }
    m_styleActionGroup = new QActionGroup(this);
    m_styleActionGroup->setExclusive(true);
    for (QAction *a : {m_actionNormal, m_actionTitleStyle, m_actionHeading1, m_actionHeading2,
                       m_actionHeading3, m_actionHeading4, m_actionQuoteStyle}) {
        m_styleActionGroup->addAction(a);
    }
    m_actionNormal->setChecked(true);
    m_actionNormal->setData(int(StyleUtils::StyleId::Normal));
    m_actionTitleStyle->setData(int(StyleUtils::StyleId::Title));
    m_actionHeading1->setData(int(StyleUtils::StyleId::Heading1));
    m_actionHeading2->setData(int(StyleUtils::StyleId::Heading2));
    m_actionHeading3->setData(int(StyleUtils::StyleId::Heading3));
    m_actionHeading4->setData(int(StyleUtils::StyleId::Heading4));
    m_actionQuoteStyle->setData(int(StyleUtils::StyleId::Quote));

    m_actionInsertImage = new QAction(tr("图片"), this);
    m_actionImageProperties = new QAction(tr("图片属性..."), this);
    m_actionInsertFormula = new QAction(tr("公式"), this);
    m_actionInsertFormula->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal));
    m_actionInsertTable = new QAction(tr("表格"), this);
    m_actionInsertTextBox = new QAction(tr("文本框"), this);
    m_actionTableAddRow = new QAction(tr("在下方插入行"), this);
    m_actionTableAddRowAbove = new QAction(tr("在上方插入行"), this);
    m_actionTableAddCol = new QAction(tr("在右侧插入列"), this);
    m_actionTableAddColLeft = new QAction(tr("在左侧插入列"), this);
    m_actionTableDelRow = new QAction(tr("删除行"), this);
    m_actionTableDelCol = new QAction(tr("删除列"), this);
    m_actionTableDelete = new QAction(tr("删除表格"), this);
    m_actionTableMerge = new QAction(tr("合并单元格"), this);
    m_actionTableSplit = new QAction(tr("拆分单元格"), this);
    m_actionTableHeader = new QAction(tr("表头样式"), this);
    m_actionTableClearHeader = new QAction(tr("清除表头样式"), this);
    m_actionTableSelectRow = new QAction(tr("选择行"), this);
    m_actionTableSelectCol = new QAction(tr("选择列"), this);
    m_actionTableSelect = new QAction(tr("选择表格"), this);
    m_actionTableCellBg = new QAction(tr("单元格底色"), this);
    m_actionTableEvenCols = new QAction(tr("平均分布列宽"), this);
    m_actionTableColumnWidths = new QAction(tr("列宽…"), this);
    m_actionTableRowHeight = new QAction(tr("行高…"), this);
    m_actionTableBandRows = new QAction(tr("隔行底色"), this);
    m_actionTableProperties = new QAction(tr("表格属性…"), this);
    m_actionInsertLine = new QAction(tr("分隔线"), this);
    m_actionInsertPageBreak = new QAction(tr("分页符"), this);
    m_actionInsertSectionBreak = new QAction(tr("分节符"), this);
    m_actionInsertFootnote = new QAction(tr("脚注"), this);
    m_actionInsertEndnote = new QAction(tr("尾注"), this);
    m_actionInsertComment = new QAction(tr("批注"), this);
    m_actionShowComments = new QAction(tr("查看批注"), this);
    m_actionInsertDate = new QAction(tr("日期"), this);
    m_actionInsertTime = new QAction(tr("时间"), this);
    m_actionInsertLink = new QAction(tr("超链接"), this);
    m_actionInsertLink->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    m_actionInsertToc = new QAction(tr("目录"), this);
    m_actionUpper = new QAction(tr("全部大写"), this);
    m_actionLower = new QAction(tr("全部小写"), this);
    m_actionTitleCase = new QAction(tr("首字母大写"), this);
    m_actionFormatPainter = new QAction(tr("格式刷"), this);
    m_actionFormatPainter->setCheckable(true);
    m_actionFormatPainter->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    m_actionOutline = new QAction(tr("导航窗格"), this);
    m_actionOutline->setCheckable(true);
    m_actionOutline->setChecked(false);
    m_actionOutline->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_actionShowRuler = new QAction(tr("标尺"), this);
    m_actionShowRuler->setCheckable(true);
    m_actionShowRuler->setChecked(true);
    m_actionShowGrid = new QAction(tr("网格线"), this);
    m_actionShowGrid->setCheckable(true);
    m_actionShowGrid->setChecked(false);
    m_actionShowRibbon = new QAction(tr("显示功能区"), this);
    m_actionShowRibbon->setCheckable(true);
    m_actionShowRibbon->setChecked(true);
    m_actionCollapseRibbon = new QAction(tr("折叠功能区"), this);
    m_actionCollapseRibbon->setCheckable(true);
    m_actionCollapseRibbon->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F1));
    m_actionAutoSave = new QAction(tr("自动保存"), this);
    m_actionAutoSave->setCheckable(true);
    m_actionAutoSave->setChecked(true);
    m_actionShortcuts = new QAction(tr("快捷键一览"), this);

    m_actionViewPage = new QAction(tr("页面视图"), this);
    m_actionViewPage->setCheckable(true);
    m_actionViewPage->setChecked(true);
    m_actionViewDraft = new QAction(tr("草稿视图"), this);
    m_actionViewDraft->setCheckable(true);
    m_actionViewOutline = new QAction(tr("大纲视图"), this);
    m_actionViewOutline->setCheckable(true);
    m_actionViewReading = new QAction(tr("阅读视图"), this);
    m_actionViewReading->setCheckable(true);
    m_actionViewWeb = new QAction(tr("Web 版式"), this);
    m_actionViewWeb->setCheckable(true);

    m_actionPagedEditorPrototype = new QAction(tr("真实分页编辑（原型）…"), this);

    auto *viewGroup = new QActionGroup(this);
    viewGroup->setExclusive(true);
    viewGroup->addAction(m_actionViewPage);
    viewGroup->addAction(m_actionViewDraft);
    viewGroup->addAction(m_actionViewOutline);
    viewGroup->addAction(m_actionViewReading);
    viewGroup->addAction(m_actionViewWeb);

    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    for (const AppStyle::ThemeInfo &info : AppStyle::availableThemes()) {
        QString title;
        switch (info.id) {
        case AppStyle::ThemeId::ClassicBlue:
            title = tr("经典蓝");
            break;
        case AppStyle::ThemeId::Graphite:
            title = tr("石墨灰");
            break;
        case AppStyle::ThemeId::Forest:
            title = tr("森绿");
            break;
        case AppStyle::ThemeId::Ocean:
            title = tr("海青");
            break;
        case AppStyle::ThemeId::Midnight:
            title = tr("午夜");
            break;
        }
        auto *action = new QAction(title, this);
        action->setCheckable(true);
        action->setData(static_cast<int>(info.id));
        if (info.id == AppStyle::currentTheme())
            action->setChecked(true);
        m_themeGroup->addAction(action);
    }

    m_actionZoomIn = new QAction(tr("放大"), this);
    m_actionZoomIn->setShortcut(QKeySequence::ZoomIn);
    m_actionZoomOut = new QAction(tr("缩小"), this);
    m_actionZoomOut->setShortcut(QKeySequence::ZoomOut);
    m_actionZoomReset = new QAction(tr("100%"), this);
    m_actionZoomReset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    m_actionAbout = new QAction(tr("关于 NewWord"), this);

    m_fontCombo = new LazyFontComboBox(this);
    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->setEditable(true);
    m_sizeCombo->setInsertPolicy(QComboBox::NoInsert);
    m_sizeCombo->setMaxVisibleItems(16);
    for (int size : QFontDatabase::standardSizes())
        m_sizeCombo->addItem(QString::number(size));
    m_sizeCombo->setCurrentText(QStringLiteral("12"));

    m_lineSpacingCombo = new QComboBox(this);
    m_lineSpacingCombo->addItem(tr("行距 1.0"), 1.0);
    m_lineSpacingCombo->addItem(tr("行距 1.15"), 1.15);
    m_lineSpacingCombo->addItem(tr("行距 1.5"), 1.5);
    m_lineSpacingCombo->addItem(tr("行距 2.0"), 2.0);
    m_lineSpacingCombo->setCurrentIndex(1);
}
