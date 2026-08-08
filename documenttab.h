#ifndef DOCUMENTTAB_H
#define DOCUMENTTAB_H

#include "documentviewmode.h"
#include "docxconverter.h"
#include "headerfootersettings.h"
#include "pagededitorwidget.h"
#include "pagelayout.h"
#include "pagelist.h"

#include <QLabel>
#include <QList>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <QVector>

class QAction;
class QEvent;
class QFrame;
class QLineEdit;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class RulerWidget;
class SpellHighlighter;
class OutlineViewWidget;

class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(QWidget *parent = nullptr);
    ~DocumentTab() override;

    PagedEditorWidget *editor() const { return m_editor; }
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);
    QString displayName() const;
    bool isModified() const;

    //! Stable id used for local draft / crash recovery snapshots.
    QString recoveryId() const { return m_recoveryId; }
    void applyEditorDefaults();
    void loadRecoveryContent(const QString &html,
                             const HeaderFooterSettings &headerFooter,
                             const PageLayoutSettings &pageLayout,
                             const QString &sourcePath = QString());

    QString headerText() const { return m_headerFooter.header; }
    QString footerText() const { return m_headerFooter.footer; }
    bool showPageNumber() const { return m_headerFooter.showPageNumber; }
    HeaderFooterSettings headerFooter() const { return m_headerFooter; }
    void setHeaderFooter(const HeaderFooterSettings &settings);
    void setHeaderFooter(const QString &header, const QString &footer, bool showPageNumber);

    PageLayoutSettings pageLayout() const { return m_pageLayout; }
    void setPageLayout(const PageLayoutSettings &layout);
    QMarginsF marginsMm() const { return m_pageLayout.marginsMm; }

    int zoomPercent() const { return m_zoomPercent; }
    void setZoomPercent(int percent);

    bool spellCheckEnabled() const { return m_spellCheckEnabled; }
    void setSpellCheckEnabled(bool enabled);

    DocumentViewMode viewMode() const { return m_viewMode; }
    void setViewMode(DocumentViewMode mode);

    void setRulerVisible(bool visible);
    [[nodiscard]] bool isRulerVisible() const { return m_rulerVisible; }
    void setGridLinesVisible(bool visible);
    [[nodiscard]] bool gridLinesVisible() const { return m_showGridLines; }

    //! Insert a floating text box on the current page (page view).
    void insertFloatingTextBox();
    void reloadFloatingTextBoxes();

    //! Live page-view metrics (1-based display uses currentPageIndex()+1).
    [[nodiscard]] int pageCount() const { return m_editor ? m_editor->pageCount() : 1; }
    [[nodiscard]] int currentPageIndex() const
    {
        return m_editor ? m_editor->currentPageIndex() : 0;
    }
    [[nodiscard]] int pageBodyHeightPx() const
    {
        return m_editor ? m_editor->pageBodyHeight() : 0;
    }

    //! Suspend spell + emit suppression helpers around bulk document replacement.
    void beginBulkDocumentUpdate();
    void endBulkDocumentUpdate();
    [[nodiscard]] bool loadFromFile(const QString &fileName, QString *errorMessage = nullptr);
    //! Apply off-thread DOCX prepare result (bridge HTML or builtin-on-GUI fallback).
    [[nodiscard]] bool loadFromPreparedDocx(const DocxConverter::PrepareResult &prepared,
                                            const QString &fileName,
                                            QString *errorMessage = nullptr);
    [[nodiscard]] bool saveToFile(const QString &fileName, QString *errorMessage = nullptr);

    void setTableContextActions(const QList<QAction *> &actions);
    void refreshTheme();
    void insertImageFile(const QString &fileName);

signals:
    void modificationChanged(bool modified);
    void cursorMoved();
    void contentsChanged();
    void zoomChanged(int percent);
    void pageLayoutChanged();
    void viewModeChanged(DocumentViewMode mode);
    void editFormulaRequested(const QString &latex, int documentPosition);
    //! Page count or current page (cursor) may have changed.
    void pageInfoChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateRulerVisibility();
    void syncRuler();
    void placeEditorInPageFrame();
    void placeEditorContinuous();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_pageHost = nullptr;
    QWidget *m_continuousHost = nullptr;
    OutlineViewWidget *m_outlineView = nullptr;

    RulerWidget *m_ruler = nullptr;
    QVBoxLayout *m_continuousLayout = nullptr;
    PagedEditorWidget *m_editor = nullptr;
    SpellHighlighter *m_highlighter = nullptr;

    QString m_filePath;
    QString m_recoveryId;
    HeaderFooterSettings m_headerFooter;
    PageLayoutSettings m_pageLayout;
    int m_zoomPercent = 100;
    bool m_spellCheckEnabled = true;
    DocumentViewMode m_viewMode = DocumentViewMode::Page;
    bool m_rulerVisible = true;
    bool m_showGridLines = false;
    QList<QAction *> m_tableActions;
};

#endif // DOCUMENTTAB_H
