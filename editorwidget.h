#ifndef EDITORWIDGET_H
#define EDITORWIDGET_H

#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QTextEdit>
#include <QVector>

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QTextTable;

class EditorWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit EditorWidget(QWidget *parent = nullptr);

    void setPageLayoutMetrics(int pageContentHeightPx, bool showPageBreaks);
    //! Engine-driven seams: document positions where a new page starts (pages 1..n-1).
    void setPageBreakDocPositions(const QVector<int> &positions);
    [[nodiscard]] QVector<int> pageBreakDocPositions() const { return m_pageBreakDocPositions; }

    void setGridLinesVisible(bool visible);
    [[nodiscard]] bool gridLinesVisible() const { return m_showGridLines; }
    void setGridSpacingPx(int spacingPx);

signals:
    void imageDropped(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct ColumnResizeSession {
        bool active = false;
        QPointer<QTextTable> table;
        int borderAfterColumn = -1; //!< drag edge between col and col+1
        QVector<qreal> startPercents;
        qreal startDocX = 0;
        qreal tableWidth = 0;
        qreal guideDocX = 0;
    };

    [[nodiscard]] QPointF viewportToDocument(const QPoint &viewportPos) const;
    [[nodiscard]] QPointF documentToViewport(const QPointF &docPos) const;
    //! Returns border index after column (0..cols-2), or -1.
    int hitTestColumnBorder(const QPoint &viewportPos, QTextTable **tableOut,
                            QRectF *tableRectOut = nullptr) const;
    void updateColumnResizeCursor(const QPoint &viewportPos);
    void applyColumnResizeDrag(const QPoint &viewportPos);

    void paintGridLines(QPainter *painter) const;

    int m_pageContentHeightPx = 0;
    bool m_showPageBreaks = false;
    bool m_showGridLines = false;
    int m_gridSpacingPx = 19; //!< ~5 mm at 100% zoom
    QVector<int> m_pageBreakDocPositions;
    ColumnResizeSession m_columnResize;
    bool m_hoveringColumnBorder = false;
};

#endif // EDITORWIDGET_H
