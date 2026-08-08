#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextTable>

#include "appstyle.h"
#include "docxconverter.h"
#include "textstats.h"

#include <atomic>
#include <memory>

class QAction;
class QActionGroup;
class QComboBox;
class LazyFontComboBox;
class QLabel;
class QMenu;
class QSlider;
class QSplitter;
class QTabWidget;
class QTimer;
class DocumentTab;
class FindReplaceDialog;
class OutlinePane;
class PagedEditorWidget;
class RibbonBar;
class PdfViewTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void closeCurrentTab();
    void exportOdt();
    void exportPdf();
    void printDocument();
    void pagePreview();
    void editHeaderFooter();
    void pageSetup();
    void editParagraph();
    void openRecentFile();
    void clearRecentFiles();

    void textBold();
    void textItalic();
    void textUnderline();
    void textStrikeout();
    void textSuperScript();
    void textSubScript();
    void textFamily(const QString &family);
    void textSize(const QString &size);
    void textColor();
    void textHighlight();
    void clearFormatting();
    void setHeading(int level);
    void applyDocumentStyle();
    void toggleBulletList();
    void toggleNumberedList();
    void increaseIndent();
    void decreaseIndent();
    void setLineSpacing(qreal factor);
    void increaseFontSize();
    void decreaseFontSize();
    void toUpperCase();
    void toLowerCase();
    void toTitleCase();

    void insertImage();
    void editImageProperties();
    void insertFormula();
    void editFormula(const QString &latex, int documentPosition = -1);
    void insertTable();
    void insertTextBox();
    void insertTableRow();
    void insertTableRowAbove();
    void insertTableColumn();
    void insertTableColumnLeft();
    void removeTableRow();
    void removeTableColumn();
    void deleteTable();
    void splitTableCells();
    void selectTableRow();
    void selectTableColumn();
    void selectTable();
    void setTableCellBackground();
    void evenTableColumns();
    void editTableColumnWidths();
    void editTableRowHeight();
    void bandTableRows();
    void tableProperties();
    void insertHorizontalLine();
    void insertPageBreak();
    void insertSectionBreak();
    void insertFootnote();
    void insertEndnote();
    void insertComment();
    void showComments();
    void insertDate();
    void insertTime();
    void insertLink();
    void insertTableOfContents();
    void pastePlainText();
    void pickFormat();
    void applyPickedFormat();
    void mergeTableCells();
    void styleTableHeader();
    void clearTableHeader();
    void toggleOutline(bool visible);
    void toggleRuler(bool visible);
    void toggleGridLines(bool visible);
    void toggleAutoSave(bool enabled);
    void autoSaveTick();
    void navigateOutline(int position);
    void showShortcuts();

    void zoomIn();
    void zoomOut();
    void zoomReset();
    void setZoomPercent(int percent);
    void toggleSpellCheck(bool enabled);
    void setDocumentViewMode();
    void openPagedEditorPrototype();

    void showFindReplace();
    void showDocumentStats();
    void about();
    void setUiTheme(QAction *action);

    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onDocumentModified(bool modified);
    void onCursorMoved();
    void onContentsChanged();
    void currentCharFormatChanged(const QTextCharFormat &format);

private:
    void setupUi();
    void setupActions();
    void setupMenus();
    void setupRibbon();
    void setupStatusBar();
    void setupConnections();
    void promptRecoverDrafts();
    void restoreSessionFiles();
    void saveSessionFiles();
    void finishDeferredStartup();
    bool writeTabDraft(DocumentTab *tab, QString *errorMessage = nullptr);
    void clearTabDraft(DocumentTab *tab);

    DocumentTab *currentTab() const;
    PdfViewTab *currentPdfTab() const;
    PagedEditorWidget *currentEditor() const;
    DocumentTab *createTab(const QString &filePath = QString());
    DocumentTab *blankStarterTab() const;
    void openDocxAsync(const QString &filePath);
    void finishDocxOpen(const QString &filePath, const DocxConverter::PrepareResult &prepared);
    void pumpPendingDocxOpens();
    PdfViewTab *createPdfTab(const QString &filePath);
    void bindTab(DocumentTab *tab);
    void updateTabTitle(DocumentTab *tab);
    void updatePdfTabTitle(PdfViewTab *tab);
    void updateWindowTitle();
    void updateUiForCurrentTab();
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString &fileName);
    void loadRecentFiles();
    void saveRecentFiles();

    void mergeFormatOnWordOrSelection(const QTextCharFormat &format);
    void changeCase(int mode);
    void fontChanged(const QFont &font);
    void alignmentChanged(Qt::Alignment alignment);
    void updateStyleActions();
    void updateListActions();
    void updateTableActions();
    void updateZoomUi(int percent);
    void syncViewModeActions();
    void applyUiTheme(AppStyle::ThemeId id);
    void applyHeaderStyleToRow(QTextTable *table, int row, bool enable);
    QTextTable *currentTable() const;
    QTextTableCell currentTableCell() const;

    bool maybeSave(DocumentTab *tab);
    bool maybeSaveAll();
    void loadFile(const QString &fileName);
    bool saveTabToFile(DocumentTab *tab, const QString &fileName);

    void updateOutline();
    void refreshOutlineSoon();
    void refreshStatsSoon();
    void updateDocumentStats();

    RibbonBar *m_ribbon = nullptr;
    QSplitter *m_bodySplitter = nullptr;
    QTabWidget *m_docTabs = nullptr;
    OutlinePane *m_outlinePane = nullptr;
    FindReplaceDialog *m_findDialog = nullptr;
    QMenu *m_recentMenu = nullptr;
    QTimer *m_autoSaveTimer = nullptr;
    QTimer *m_outlineRefreshTimer = nullptr;
    QTimer *m_statsRefreshTimer = nullptr;
    bool m_cursorWasInTable = false;
    bool m_deferredStartupDone = false;
    TextStats::Counts m_cachedDocStats;
    int m_cachedStatsRevision = -1;

    bool m_docxOpenBusy = false;
    QStringList m_pendingDocxOpens;
    std::shared_ptr<std::atomic_bool> m_docxOpenCancel;
    class QProgressDialog *m_docxProgress = nullptr;

    QStringList m_recentFiles;
    bool m_spellCheckEnabled = true;
    bool m_autoSaveEnabled = true;
    bool m_formatPainterArmed = false;
    QTextCharFormat m_copiedCharFormat;
    QTextBlockFormat m_copiedBlockFormat;
    bool m_hasCopiedFormat = false;

    QAction *m_actionNew = nullptr;
    QAction *m_actionOpen = nullptr;
    QAction *m_actionSave = nullptr;
    QAction *m_actionSaveAs = nullptr;
    QAction *m_actionCloseTab = nullptr;
    QAction *m_actionExportOdt = nullptr;
    QAction *m_actionExportPdf = nullptr;
    QAction *m_actionPrint = nullptr;
    QAction *m_actionPreview = nullptr;
    QAction *m_actionHeaderFooter = nullptr;
    QAction *m_actionPageSetup = nullptr;
    QAction *m_actionParagraph = nullptr;
    QAction *m_actionExit = nullptr;
    QAction *m_actionClearRecent = nullptr;

    QAction *m_actionUndo = nullptr;
    QAction *m_actionRedo = nullptr;
    QAction *m_actionCut = nullptr;
    QAction *m_actionCopy = nullptr;
    QAction *m_actionPaste = nullptr;
    QAction *m_actionPastePlain = nullptr;
    QAction *m_actionFind = nullptr;
    QAction *m_actionSelectAll = nullptr;
    QAction *m_actionStats = nullptr;
    QAction *m_actionSpellCheck = nullptr;

    QAction *m_actionBold = nullptr;
    QAction *m_actionItalic = nullptr;
    QAction *m_actionUnderline = nullptr;
    QAction *m_actionStrike = nullptr;
    QAction *m_actionSuper = nullptr;
    QAction *m_actionSub = nullptr;
    QAction *m_actionTextColor = nullptr;
    QAction *m_actionHighlight = nullptr;
    QAction *m_actionClearFormat = nullptr;

    QAction *m_actionAlignLeft = nullptr;
    QAction *m_actionAlignCenter = nullptr;
    QAction *m_actionAlignRight = nullptr;
    QAction *m_actionAlignJustify = nullptr;

    QAction *m_actionBullet = nullptr;
    QAction *m_actionNumbered = nullptr;
    QAction *m_actionIndent = nullptr;
    QAction *m_actionOutdent = nullptr;
    QAction *m_actionFontInc = nullptr;
    QAction *m_actionFontDec = nullptr;

    QAction *m_actionHeading1 = nullptr;
    QAction *m_actionHeading2 = nullptr;
    QAction *m_actionHeading3 = nullptr;
    QAction *m_actionHeading4 = nullptr;
    QAction *m_actionTitleStyle = nullptr;
    QAction *m_actionQuoteStyle = nullptr;
    QAction *m_actionNormal = nullptr;
    QActionGroup *m_styleActionGroup = nullptr;

    QAction *m_actionInsertImage = nullptr;
    QAction *m_actionImageProperties = nullptr;
    QAction *m_actionInsertFormula = nullptr;
    QAction *m_actionInsertTable = nullptr;
    QAction *m_actionInsertTextBox = nullptr;
    QAction *m_actionTableAddRow = nullptr;
    QAction *m_actionTableAddRowAbove = nullptr;
    QAction *m_actionTableAddCol = nullptr;
    QAction *m_actionTableAddColLeft = nullptr;
    QAction *m_actionTableDelRow = nullptr;
    QAction *m_actionTableDelCol = nullptr;
    QAction *m_actionTableDelete = nullptr;
    QAction *m_actionTableMerge = nullptr;
    QAction *m_actionTableSplit = nullptr;
    QAction *m_actionTableHeader = nullptr;
    QAction *m_actionTableClearHeader = nullptr;
    QAction *m_actionTableSelectRow = nullptr;
    QAction *m_actionTableSelectCol = nullptr;
    QAction *m_actionTableSelect = nullptr;
    QAction *m_actionTableCellBg = nullptr;
    QAction *m_actionTableEvenCols = nullptr;
    QAction *m_actionTableColumnWidths = nullptr;
    QAction *m_actionTableRowHeight = nullptr;
    QAction *m_actionTableBandRows = nullptr;
    QAction *m_actionTableProperties = nullptr;
    QAction *m_actionInsertLine = nullptr;
    QAction *m_actionInsertPageBreak = nullptr;
    QAction *m_actionInsertSectionBreak = nullptr;
    QAction *m_actionInsertFootnote = nullptr;
    QAction *m_actionInsertEndnote = nullptr;
    QAction *m_actionInsertComment = nullptr;
    QAction *m_actionShowComments = nullptr;
    QAction *m_actionInsertDate = nullptr;
    QAction *m_actionInsertTime = nullptr;
    QAction *m_actionInsertLink = nullptr;
    QAction *m_actionInsertToc = nullptr;
    QAction *m_actionUpper = nullptr;
    QAction *m_actionLower = nullptr;
    QAction *m_actionTitleCase = nullptr;
    QAction *m_actionFormatPainter = nullptr;
    QAction *m_actionOutline = nullptr;
    QAction *m_actionShowRuler = nullptr;
    QAction *m_actionShowGrid = nullptr;
    QAction *m_actionShowRibbon = nullptr;
    QAction *m_actionCollapseRibbon = nullptr;
    QAction *m_actionAutoSave = nullptr;
    QAction *m_actionShortcuts = nullptr;
    QAction *m_actionViewPage = nullptr;
    QAction *m_actionViewDraft = nullptr;
    QAction *m_actionViewOutline = nullptr;
    QAction *m_actionViewReading = nullptr;
    QAction *m_actionViewWeb = nullptr;
    QAction *m_actionPagedEditorPrototype = nullptr;
    QAction *m_actionZoomIn = nullptr;
    QAction *m_actionZoomOut = nullptr;
    QAction *m_actionZoomReset = nullptr;
    QAction *m_actionAbout = nullptr;
    QActionGroup *m_themeGroup = nullptr;

    LazyFontComboBox *m_fontCombo = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    QComboBox *m_lineSpacingCombo = nullptr;
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
};

#endif // MAINWINDOW_H
