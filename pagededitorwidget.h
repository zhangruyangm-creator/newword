#ifndef PAGEDEDITORWIDGET_H
#define PAGEDEDITORWIDGET_H

#include "floatingtextbox.h"
#include "headerfootersettings.h"
#include "pagedfloatingboxes.h"
#include "pagedocumentlayout.h"
#include "pagelayout.h"

#include <QHash>
#include <QPointer>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextCursor>
#include <QVector>
#include <QWidget>

class QDragEnterEvent;
class QDropEvent;
class QMenu;
class PagedImageObject;
class QScrollBar;
class QTextDocument;
class QTextTable;
class QTimer;

//! Self-drawn, truly paginated text editor (Word-style print layout).
//!
//! QTextEdit cannot be paginated: it forces its document into continuous flow
//! (pageSize height = -1) on every relayout. This widget instead owns the
//! painting, hit-testing, input method and clipboard handling itself, while
//! the underlying QTextDocument keeps the real page layout
//! (pageSize = content-box width x content-box height).
class PagedEditorWidget : public QWidget
{
    Q_OBJECT

public:
    PagedEditorWidget(QTextDocument *document,
                      const PageLayoutSettings &layout,
                      const HeaderFooterSettings &headerFooter,
                      QWidget *parent = nullptr);
    ~PagedEditorWidget() override;

    QTextDocument *document() const { return m_document; }
    QTextCursor textCursor() const { return m_cursor; }
    void setTextCursor(const QTextCursor &cursor);

    [[nodiscard]] int pageCount() const { return m_pageCount; }
    [[nodiscard]] int currentPageIndex() const; // 0-based page containing the cursor
    //! Content-box height of one page at 100% zoom (page view metrics).
    [[nodiscard]] int pageBodyHeight() const { return qMax(1, qRound(m_contentHeight)); }
    [[nodiscard]] int pageContentWidth() const { return qMax(1, qRound(m_contentWidth)); }
    [[nodiscard]] int scrollValue() const;
    //! Horizontal offset (widget coords) of the centered page — ruler alignment.
    [[nodiscard]] int pageOffsetX() const;

    //! Qt's HTML import yields documents whose later edits are O(n) per
    //! keystroke. Rebuilding the same content through the cursor API restores
    //! the fast edit path. Preserves resources (images/formulas) and formats.
    static void normalizeDocumentStructure(QTextDocument *document);
    //! Cap oversized image resources so memory and paint cost stay bounded.
    static void downscaleImageResources(QTextDocument *document);
    //! Floating text boxes (overlay objects anchored to pages).
    void reloadFloatingTextBoxes();
    void insertFloatingTextBox(const FloatingTextBox &box);
    void removeFloatingTextBox(const QString &id);
    //! 5 mm grid overlay (page view helper).
    void setGridLinesVisible(bool visible);
    [[nodiscard]] bool gridLinesVisible() const { return m_showGrid; }
    void setGridSpacingPx(int spacingPx) { m_gridSpacingPx = qMax(4, spacingPx); }

    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();

    // ---- QTextEdit-compatible editing surface used by the main window ----
    [[nodiscard]] QString toPlainText() const;
    [[nodiscard]] QString toHtml() const;
    void setHtml(const QString &html);
    void setPlainText(const QString &text);
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }
    void setUndoRedoEnabled(bool enabled);
    void setTabStopDistance(qreal distance);
    void setAcceptRichText(bool accept) { m_acceptRichText = accept; }
    [[nodiscard]] bool acceptRichText() const { return m_acceptRichText; }
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QFont currentFont() const;
    [[nodiscard]] QTextCharFormat currentCharFormat() const;
    void setCurrentCharFormat(const QTextCharFormat &format);
    void mergeCurrentCharFormat(const QTextCharFormat &modifier);
    [[nodiscard]] Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);
    bool find(const QString &expression, QTextDocument::FindFlags flags = {});
    [[nodiscard]] QTextCursor cursorForPosition(const QPoint &pos) const;
    QMenu *createStandardContextMenu();
    //! The paged editor has no separate viewport; keep QTextEdit call sites working.
    QWidget *viewport() { return this; }
    void ensureCursorVisible();

    // ---- Page configuration ----
    void setPageLayout(const PageLayoutSettings &layout);
    PageLayoutSettings pageLayout() const { return m_layout; }
    void setHeaderFooter(const HeaderFooterSettings &settings);
    HeaderFooterSettings headerFooter() const { return m_headerFooter; }
    void setZoomPercent(int percent);
    [[nodiscard]] int zoomPercent() const { return m_zoomPercent; }
    //! true = real paper pages; false = continuous strip (draft/web views).
    void setPageMode(bool pageMode);
    [[nodiscard]] bool pageMode() const { return m_pageMode; }

    //! Cursor rectangle in widget coordinates (used for IME and blink drawing).
    [[nodiscard]] QRectF cursorRect() const;

signals:
    void cursorPositionChanged();
    void pageInfoChanged();
    void currentCharFormatChanged(const QTextCharFormat &format);
    void copyAvailable(bool yes);
    void selectionChanged();
    void undoAvailable(bool available);
    void redoAvailable(bool available);
    void imageDropped(const QString &filePath);
    void scrolled();
    void pageGeometryChanged();
    void floatingBoxesChanged();
    void imageDoubleClicked(int documentPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    [[nodiscard]] QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updateMetrics();
    void relayoutDocument();
    void updateScrollBar();
    void updatePageCount();

    [[nodiscard]] QRectF pageRectInWidget(int pageIndex) const;    // whole page incl. margins
    [[nodiscard]] QRectF contentRectInWidget(int pageIndex) const; // text content box
    [[nodiscard]] int pageIndexAt(const QPoint &widgetPos) const;
    [[nodiscard]] QPointF documentPointAt(const QPoint &widgetPos) const;
    void setCursorFromWidget(const QPoint &widgetPos, bool keepAnchor);

    void drawPage(QPainter *painter, int pageIndex);
    void drawHeaderFooter(QPainter *painter, int pageIndex, const QRectF &pageRect);
    void paintGridLines(QPainter *painter, const QRectF &contentRect);
    [[nodiscard]] int hitTestColumnBorder(const QPoint &widgetPos, QTextTable **tableOut,
                                          QRectF *tableRectOut = nullptr) const;
    void updateColumnResizeCursor(const QPoint &widgetPos);
    void applyColumnResizeDrag(const QPoint &widgetPos);

    void moveCursor(QTextCursor::MoveOperation op, QTextCursor::MoveMode mode, int n = 1);
    void insertText(const QString &text);
    void deleteChar(bool backspace);
    void removePreedit();
    void insertPreedit(const QString &text);
    void scrollBy(int deltaY);
    void resetCursorBlink();
    void afterCursorMove();
    void afterDocumentChange();
    void updateEditRegion();
    [[nodiscard]] qreal zoomScale() const;

    QTextDocument *m_document = nullptr;
    PagedDocumentLayout m_layoutModel; //!< pagination model (ranges / incremental)
    PageLayoutSettings m_layout;
    HeaderFooterSettings m_headerFooter;
    QTextCursor m_cursor;
    QTextCharFormat m_currentCharFormat;
    QTextCharFormat m_preeditBaseFormat; //!< format captured before composition starts
    PagedImageObject *m_imageObject = nullptr;
    QScrollBar *m_vScroll = nullptr;
    QTimer *m_blinkTimer = nullptr;
    QTimer *m_autoScrollTimer = nullptr;
    QTimer *m_fullPassTimer = nullptr;
    QTimer *m_formatCheckTimer = nullptr;
    QPoint m_lastMousePos;
    bool m_cursorVisible = true;
    bool m_hasSelectionDrag = false;
    bool m_dragging = false;
    bool m_readOnly = false;
    bool m_acceptRichText = true;
    bool m_pageMode = true;
    bool m_lastHadSelection = false;
    bool m_burstHasChars = false;
    bool m_burstHasFormatOnly = false;
    int m_preeditLength = 0;
    int m_zoomPercent = 100;
    int m_lastChangePos = -1;

    qreal m_pageWidth = 0;
    qreal m_pageHeight = 0;
    qreal m_contentLeft = 0;
    qreal m_contentTop = 0;
    qreal m_contentWidth = 0;
    qreal m_contentHeight = 0;
    qreal m_gap = 28;
    qreal m_topPad = 24;
    mutable int m_pageCount = 1;

    PagedFloatingBoxes m_floatBoxes;

    bool m_showGrid = false;
    int m_gridSpacingPx = 19; //!< 5 mm at 100% zoom
    bool m_hoveringColumnBorder = false;

    struct ColumnResizeSession {
        bool active = false;
        QPointer<QTextTable> table;
        int borderAfterColumn = -1;
        QVector<qreal> startPercents;
        qreal startDocX = 0;
        qreal tableWidth = 0;
        qreal guideDocX = 0;
    };
    ColumnResizeSession m_columnResize;
};

#endif // PAGEDEDITORWIDGET_H
