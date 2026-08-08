#include "editorwidget.h"
#include "tablegeometry.h"

#include <QAbstractTextDocumentLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextTable>
#include <QUrl>

EditorWidget::EditorWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptDrops(true);
    viewport()->setMouseTracking(true);
    setMouseTracking(true);
}

void EditorWidget::setPageLayoutMetrics(int pageContentHeightPx, bool showPageBreaks)
{
    m_pageContentHeightPx = pageContentHeightPx;
    m_showPageBreaks = showPageBreaks;
    viewport()->update();
}

void EditorWidget::setPageBreakDocPositions(const QVector<int> &positions)
{
    if (m_pageBreakDocPositions == positions)
        return;
    m_pageBreakDocPositions = positions;
    viewport()->update();
}

void EditorWidget::setGridLinesVisible(bool visible)
{
    if (m_showGridLines == visible)
        return;
    m_showGridLines = visible;
    viewport()->update();
}

void EditorWidget::setGridSpacingPx(int spacingPx)
{
    spacingPx = qMax(4, spacingPx);
    if (m_gridSpacingPx == spacingPx)
        return;
    m_gridSpacingPx = spacingPx;
    if (m_showGridLines)
        viewport()->update();
}

QPointF EditorWidget::viewportToDocument(const QPoint &viewportPos) const
{
    return QPointF(viewportPos.x() + horizontalScrollBar()->value(),
                   viewportPos.y() + verticalScrollBar()->value());
}

QPointF EditorWidget::documentToViewport(const QPointF &docPos) const
{
    return QPointF(docPos.x() - horizontalScrollBar()->value(),
                   docPos.y() - verticalScrollBar()->value());
}

int EditorWidget::hitTestColumnBorder(const QPoint &viewportPos, QTextTable **tableOut,
                                      QRectF *tableRectOut) const
{
    if (tableOut)
        *tableOut = nullptr;
    if (tableRectOut)
        *tableRectOut = {};

    QTextCursor cursor = cursorForPosition(viewportPos);
    QTextTable *table = cursor.currentTable();
    if (!table || table->columns() < 2)
        return -1;

    auto *layout = document()->documentLayout();
    if (!layout)
        return -1;

    const QRectF tableRect = layout->frameBoundingRect(table);
    if (!tableRect.isValid() || tableRect.height() < 2.0)
        return -1;

    const QPointF docPos = viewportToDocument(viewportPos);
    constexpr qreal kYPad = 2.0;
    if (docPos.y() < tableRect.top() - kYPad || docPos.y() > tableRect.bottom() + kYPad)
        return -1;

    const QVector<qreal> edges = TableGeometry::columnEdgeXs(table, tableRect);
    if (edges.size() < 3)
        return -1;

    constexpr qreal kTolerance = 5.0;
    int best = -1;
    qreal bestDist = kTolerance;
    // Internal borders only (between columns).
    for (int i = 1; i + 1 < edges.size(); ++i) {
        const qreal dist = qAbs(docPos.x() - edges.at(i));
        if (dist <= bestDist) {
            bestDist = dist;
            best = i - 1; // border after column
        }
    }
    if (best < 0)
        return -1;

    if (tableOut)
        *tableOut = table;
    if (tableRectOut)
        *tableRectOut = tableRect;
    return best;
}

void EditorWidget::updateColumnResizeCursor(const QPoint &viewportPos)
{
    if (m_columnResize.active)
        return;
    QTextTable *table = nullptr;
    const int border = hitTestColumnBorder(viewportPos, &table);
    const bool hover = border >= 0 && table;
    if (hover) {
        viewport()->setCursor(Qt::SplitHCursor);
    } else if (m_hoveringColumnBorder) {
        viewport()->unsetCursor();
    }
    m_hoveringColumnBorder = hover;
}

void EditorWidget::applyColumnResizeDrag(const QPoint &viewportPos)
{
    if (!m_columnResize.active || !m_columnResize.table)
        return;
    if (m_columnResize.tableWidth < 8.0)
        return;

    const QPointF docPos = viewportToDocument(viewportPos);
    const qreal deltaPx = docPos.x() - m_columnResize.startDocX;
    const qreal deltaPct = deltaPx / m_columnResize.tableWidth * 100.0;

    QVector<qreal> percents = m_columnResize.startPercents;
    const int left = m_columnResize.borderAfterColumn;
    const int right = left + 1;
    if (left < 0 || right >= percents.size())
        return;

    constexpr qreal kMinPct = 5.0;
    qreal leftPct = m_columnResize.startPercents.at(left) + deltaPct;
    qreal rightPct = m_columnResize.startPercents.at(right) - deltaPct;
    if (leftPct < kMinPct) {
        rightPct -= (kMinPct - leftPct);
        leftPct = kMinPct;
    }
    if (rightPct < kMinPct) {
        leftPct -= (kMinPct - rightPct);
        rightPct = kMinPct;
    }
    if (leftPct < kMinPct || rightPct < kMinPct)
        return;

    percents[left] = leftPct;
    percents[right] = rightPct;
    TableGeometry::setColumnWidthPercents(m_columnResize.table, percents);

    m_columnResize.guideDocX = docPos.x();
    viewport()->update();
}

void EditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QTextTable *table = nullptr;
        QRectF tableRect;
        const int border = hitTestColumnBorder(event->pos(), &table, &tableRect);
        if (border >= 0 && table) {
            m_columnResize.active = true;
            m_columnResize.table = table;
            m_columnResize.borderAfterColumn = border;
            m_columnResize.startPercents = TableGeometry::columnWidthPercents(table);
            m_columnResize.startDocX = viewportToDocument(event->pos()).x();
            m_columnResize.tableWidth = tableRect.width();
            m_columnResize.guideDocX = m_columnResize.startDocX;
            viewport()->setCursor(Qt::SplitHCursor);
            event->accept();
            return;
        }
    }
    QTextEdit::mousePressEvent(event);
}

void EditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_columnResize.active) {
        applyColumnResizeDrag(event->pos());
        event->accept();
        return;
    }
    QTextTable *table = nullptr;
    const int border = hitTestColumnBorder(event->pos(), &table);
    if (border >= 0 && table) {
        viewport()->setCursor(Qt::SplitHCursor);
        m_hoveringColumnBorder = true;
        event->accept();
        return;
    }
    if (m_hoveringColumnBorder) {
        viewport()->unsetCursor();
        m_hoveringColumnBorder = false;
    }
    QTextEdit::mouseMoveEvent(event);
}

void EditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_columnResize.active && event->button() == Qt::LeftButton) {
        applyColumnResizeDrag(event->pos());
        m_columnResize = {};
        updateColumnResizeCursor(event->pos());
        event->accept();
        return;
    }
    QTextEdit::mouseReleaseEvent(event);
}

void EditorWidget::leaveEvent(QEvent *event)
{
    if (!m_columnResize.active && m_hoveringColumnBorder) {
        viewport()->unsetCursor();
        m_hoveringColumnBorder = false;
    }
    QTextEdit::leaveEvent(event);
}

void EditorWidget::paintGridLines(QPainter *painter) const
{
    if (!painter || m_gridSpacingPx < 4)
        return;

    const int spacing = m_gridSpacingPx;
    const int w = viewport()->width();
    const int h = viewport()->height();
    const int scrollX = horizontalScrollBar()->value();
    const int scrollY = verticalScrollBar()->value();

    painter->setPen(QPen(QColor(190, 198, 210, 110), 1));

    const int firstDocX = (scrollX / spacing) * spacing;
    for (int docX = firstDocX; ; docX += spacing) {
        const int vx = docX - scrollX;
        if (vx > w)
            break;
        if (vx >= 0)
            painter->drawLine(vx, 0, vx, h);
    }

    const int firstDocY = (scrollY / spacing) * spacing;
    for (int docY = firstDocY; ; docY += spacing) {
        const int vy = docY - scrollY;
        if (vy > h)
            break;
        if (vy >= 0)
            painter->drawLine(0, vy, w, vy);
    }
}

void EditorWidget::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);

    // Draw after text so opaque draft/web backgrounds do not cover the grid.
    // Lines stay light enough to read through.
    if (m_showGridLines) {
        QPainter gridPainter(viewport());
        paintGridLines(&gridPainter);
    }

    if (m_columnResize.active && m_columnResize.table) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        auto *layout = document()->documentLayout();
        if (layout) {
            const QRectF tableRect = layout->frameBoundingRect(m_columnResize.table);
            const QPointF top = documentToViewport(QPointF(m_columnResize.guideDocX, tableRect.top()));
            const QPointF bottom =
                documentToViewport(QPointF(m_columnResize.guideDocX, tableRect.bottom()));
            QPen pen(QColor(0, 120, 215), 1.5);
            painter.setPen(pen);
            painter.drawLine(top, bottom);
        }
    }
    if (!m_showPageBreaks)
        return;

    QPainter painter(viewport());
    QFont labelFont = painter.font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);
    const QFontMetrics fm(labelFont);

    auto drawBreakAt = [&](int vy, int pageNo) {
        if (vy < -2 || vy > viewport()->height() + 2)
            return;
        painter.setPen(QPen(QColor(170, 170, 180), 1, Qt::DashLine));
        painter.drawLine(0, vy, viewport()->width(), vy);
        painter.setPen(QColor(140, 140, 150));
        const QString label = tr("第 %1 页结束").arg(pageNo);
        const int lw = fm.horizontalAdvance(label) + 8;
        painter.fillRect(viewport()->width() - lw - 6, vy - fm.height() - 2, lw, fm.height() + 2,
                         QColor(255, 255, 255, 220));
        painter.drawText(QRect(viewport()->width() - lw - 6, vy - fm.height() - 1, lw, fm.height()),
                         Qt::AlignRight | Qt::AlignVCenter, label);
    };

    // Page seams sit on the page grid (k * page height) so every page renders
    // full height and the junction never jumps while Precise pagination catches
    // up. The engine's doc positions still drive the page count and cursor page
    // index (DocumentTab::currentPageIndex), but are not used for painting:
    // the engine and the editor lay text out with different metrics, so mapping
    // engine break offsets into the editor's layout makes pages look short.
    if (m_pageContentHeightPx <= 0)
        return;
    const int H = m_pageContentHeightPx;
    // Grid breaks stop at the actual document height so a strip that is kept
    // taller than its content (while Precise catches up) has no phantom pages.
    const int contentH = qMin(height(), qRound(document()->size().height()));
    for (int y = H; y < contentH; y += H)
        drawBreakAt(y, y / H);
}

void EditorWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage())
        event->acceptProposedAction();
    else
        QTextEdit::dragEnterEvent(event);
}

void EditorWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage())
        event->acceptProposedAction();
    else
        QTextEdit::dragMoveEvent(event);
}

void EditorWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString path = url.toLocalFile();
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (suffix == QLatin1String("png") || suffix == QLatin1String("jpg")
                || suffix == QLatin1String("jpeg") || suffix == QLatin1String("bmp")
                || suffix == QLatin1String("gif") || suffix == QLatin1String("webp")) {
                emit imageDropped(path);
                event->acceptProposedAction();
                return;
            }
        }
    }
    QTextEdit::dropEvent(event);
}
