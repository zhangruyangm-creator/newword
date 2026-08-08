#ifndef PAGEDEDITORWIDGET_H
#define PAGEDEDITORWIDGET_H

#include "headerfootersettings.h"
#include "pagelayout.h"

#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextCursor>
#include <QVector>
#include <QWidget>

class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QScrollBar;
class QTextDocument;
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
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updateMetrics();
    void relayoutDocument();
    //! Qt lays the last overflowing line of a page exactly at the page height,
    //! so pages are not on a uniform grid. We paginate with a reduced layout
    //! page height and compute each page's real doc-y range from block rects.
    void recomputePagination();
    //! Incremental pagination: after a text edit only the affected page(s) are
    //! rebuilt on demand; a debounced full pass keeps page count / scrollbar exact.
    void markDirty(int position);
    void ensureRangesThroughPage(int pageIndex) const;
    void ensurePageRangeSize(int pageIndex) const;
    [[nodiscard]] int pageIndexForDocY(qreal docY) const;
    void updateScrollBar();
    void updatePageCount();

    [[nodiscard]] QRectF pageRectInWidget(int pageIndex) const;    // whole page incl. margins
    [[nodiscard]] QRectF contentRectInWidget(int pageIndex) const; // text content box
    [[nodiscard]] int pageIndexAt(const QPoint &widgetPos) const;
    [[nodiscard]] QPointF documentPointAt(const QPoint &widgetPos) const;
    void setCursorFromWidget(const QPoint &widgetPos, bool keepAnchor);

    void drawPage(QPainter *painter, int pageIndex);
    void drawHeaderFooter(QPainter *painter, int pageIndex, const QRectF &pageRect);

    void moveCursor(QTextCursor::MoveOperation op, QTextCursor::MoveMode mode, int n = 1);
    void insertText(const QString &text);
    void deleteChar(bool backspace);
    void removePreedit();
    void insertPreedit(const QString &text);
    void scrollBy(int deltaY);
    void resetCursorBlink();
    void afterCursorMove();
    void afterDocumentChange();
    void applyContinuousLayout();
    [[nodiscard]] qreal zoomScale() const;

    QTextDocument *m_document = nullptr;
    PageLayoutSettings m_layout;
    HeaderFooterSettings m_headerFooter;
    QTextCursor m_cursor;
    QTextCharFormat m_currentCharFormat;
    QScrollBar *m_vScroll = nullptr;
    QTimer *m_blinkTimer = nullptr;
    QTimer *m_autoScrollTimer = nullptr;
    QTimer *m_fullPassTimer = nullptr;
    QPoint m_lastMousePos;
    bool m_cursorVisible = true;
    bool m_hasSelectionDrag = false;
    bool m_dragging = false;
    bool m_readOnly = false;
    bool m_acceptRichText = true;
    bool m_pageMode = true;
    bool m_lastHadSelection = false;
    bool m_maxLineHeightDirty = true;
    int m_preeditLength = 0;
    int m_zoomPercent = 100;
    int m_lastChangePos = -1;
    mutable int m_rebuildBlock = -1; //!< first stale block; -1 = ranges fully accurate

    qreal m_pageWidth = 0;
    qreal m_pageHeight = 0;
    qreal m_contentLeft = 0;
    qreal m_contentTop = 0;
    qreal m_contentWidth = 0;
    qreal m_contentHeight = 0;
    qreal m_layoutPageHeight = 0;
    qreal m_maxLineHeight = 1;
    qreal m_gap = 28;
    qreal m_topPad = 24;
    mutable int m_pageCount = 1;
    mutable bool m_recomputingPagination = false;

    struct PageRange {
        qreal start = 0;
        qreal end = 0;
    };
    mutable QVector<PageRange> m_pageRanges;
    mutable QVector<int> m_pageStartBlocks; //!< first block number per page
    mutable int m_rangeBuiltToPage = -1;    //!< pages [0..this] are accurate
};

#endif // PAGEDEDITORWIDGET_H
