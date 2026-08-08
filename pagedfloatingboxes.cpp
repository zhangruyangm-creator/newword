#include "pagedfloatingboxes.h"

#include <QAbstractTextDocumentLayout>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QTextDocument>
#include <QTextEdit>

#include <algorithm>

namespace {
constexpr qreal kPtToPx = 96.0 / 72.0;
}

PagedFloatingBoxes::PagedFloatingBoxes(QWidget *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

void PagedFloatingBoxes::reload()
{
    m_boxes = FloatingTextBoxes::load(m_document);
    m_docs.clear();
    if (!m_selectedId.isEmpty() && indexOf(m_selectedId) < 0)
        m_selectedId.clear();
    closeEditor();
}

void PagedFloatingBoxes::insert(const FloatingTextBox &box)
{
    m_boxes.append(box);
    m_selectedId = box.id;
    save();
}

void PagedFloatingBoxes::remove(const QString &id)
{
    const int idx = indexOf(id);
    if (idx < 0)
        return;
    closeEditor();
    m_boxes.removeAt(idx);
    m_docs.remove(id);
    if (m_selectedId == id)
        m_selectedId.clear();
    save();
}

void PagedFloatingBoxes::selectAt(int index)
{
    if (index >= 0 && index < m_boxes.size())
        m_selectedId = m_boxes.at(index).id;
}

bool PagedFloatingBoxes::openEditorAt(int index, qreal zoom)
{
    if (index < 0 || index >= m_boxes.size())
        return false;
    m_selectedId = m_boxes.at(index).id;
    return openEditor(m_selectedId, zoom);
}

void PagedFloatingBoxes::save()
{
    if (!m_document)
        return;
    FloatingTextBoxes::save(m_document, m_boxes, true);
    emit boxesChanged();
}

int PagedFloatingBoxes::indexOf(const QString &id) const
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes.at(i).id == id)
            return i;
    }
    return -1;
}

QRectF PagedFloatingBoxes::rectInWidget(const FloatingTextBox &box, qreal zoom) const
{
    const QRectF content = m_pageContentRect ? m_pageContentRect(box.pageIndex) : QRectF();
    return QRectF(content.left() + box.xPt * kPtToPx * zoom,
                  content.top() + box.yPt * kPtToPx * zoom,
                  box.wPt * kPtToPx * zoom,
                  box.hPt * kPtToPx * zoom);
}

int PagedFloatingBoxes::hitTest(const QPoint &widgetPos, qreal zoom) const
{
    for (int i = m_boxes.size() - 1; i >= 0; --i) {
        if (rectInWidget(m_boxes.at(i), zoom).adjusted(-2, -2, 2, 2).contains(widgetPos))
            return i;
    }
    return -1;
}

bool PagedFloatingBoxes::hoverShape(const QPoint &widgetPos, qreal zoom,
                                    Qt::CursorShape *shape) const
{
    const int idx = hitTest(widgetPos, zoom);
    if (idx < 0)
        return false;
    const QRectF r = rectInWidget(m_boxes.at(idx), zoom);
    *shape = QRectF(r.right() - 14, r.bottom() - 14, 14, 14).contains(widgetPos)
                 ? Qt::SizeFDiagCursor
                 : Qt::SizeAllCursor;
    return true;
}

void PagedFloatingBoxes::paint(QPainter *painter, int pageIndex, qreal zoom,
                               const QPalette &palette)
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        const FloatingTextBox &box = m_boxes.at(i);
        if (box.pageIndex != pageIndex)
            continue;
        const QRectF r = rectInWidget(box, zoom);
        painter->fillRect(r, QColor(255, 255, 255, 238));

        const bool selected = box.id == m_selectedId;
        QPen pen(selected ? QColor(110, 118, 132) : QColor(198, 202, 210), 1.0);
        pen.setCosmetic(true);
        if (!selected) {
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({3.0, 2.5});
        }
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));

        const QRectF content = r.adjusted(3, 3, -3, -3);
        if (content.width() > 10 && content.height() > 10) {
            QTextDocument *boxDoc = m_docs.value(box.id);
            if (!boxDoc) {
                boxDoc = new QTextDocument(this);
                boxDoc->setDocumentMargin(0);
                boxDoc->setHtml(box.html);
                m_docs.insert(box.id, boxDoc);
            }
            boxDoc->setPageSize(content.size());
            painter->save();
            painter->setClipRect(content);
            painter->translate(content.topLeft());
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.palette = palette;
            boxDoc->documentLayout()->draw(painter, ctx);
            painter->restore();
        }

        if (selected) {
            painter->fillRect(QRectF(r.right() - 7, r.bottom() - 7, 7, 7),
                              QColor(150, 156, 168));
        }
    }
}

bool PagedFloatingBoxes::beginDrag(int index, const QPoint &widgetPos, qreal zoom)
{
    if (index < 0 || index >= m_boxes.size())
        return false;
    m_selectedId = m_boxes.at(index).id;
    m_dragOrig = m_boxes.at(index);
    m_dragStart = widgetPos;
    const QRectF r = rectInWidget(m_dragOrig, zoom);
    m_dragResize = QRectF(r.right() - 14, r.bottom() - 14, 14, 14).contains(widgetPos);
    m_dragMove = !m_dragResize;
    return true;
}

bool PagedFloatingBoxes::dragTo(const QPoint &widgetPos, qreal zoom)
{
    if (!isDragging())
        return false;
    const int idx = indexOf(m_selectedId);
    if (idx < 0)
        return false;
    const qreal inv = 1.0 / (kPtToPx * zoom);
    const QPointF delta = (widgetPos - m_dragStart) * inv;
    FloatingTextBox &box = m_boxes[idx];
    if (m_dragMove) {
        box.xPt = qMax(0.0, m_dragOrig.xPt + delta.x());
        box.yPt = qMax(0.0, m_dragOrig.yPt + delta.y());
    } else {
        box.wPt = qMax(30.0, m_dragOrig.wPt + delta.x());
        box.hPt = qMax(20.0, m_dragOrig.hPt + delta.y());
    }
    return true;
}

bool PagedFloatingBoxes::endDrag(const QPoint &widgetPos, qreal zoom)
{
    if (!isDragging())
        return false;
    dragTo(widgetPos, zoom);
    m_dragMove = false;
    m_dragResize = false;
    save();
    return true;
}

bool PagedFloatingBoxes::openEditor(const QString &id, qreal zoom)
{
    const int idx = indexOf(id);
    if (idx < 0)
        return false;
    if (!m_boxEditor) {
        m_boxEditor = new QTextEdit(m_host);
        m_boxEditor->setFrameShape(QFrame::NoFrame);
        m_boxEditor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: white; border: 1px solid #8a93a3; }"));
        m_boxEditor->installEventFilter(this);
        m_boxEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_boxEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_boxEditor->hide();
    }
    m_editingId = id;
    const FloatingTextBox &box = m_boxes.at(idx);
    m_boxEditor->setGeometry(rectInWidget(box, zoom).toRect().adjusted(1, 1, -1, -1));
    m_boxEditor->setHtml(box.html);
    m_boxEditor->show();
    m_boxEditor->raise();
    m_boxEditor->setFocus();
    return true;
}

void PagedFloatingBoxes::commitEditor()
{
    if (!m_boxEditor || m_editingId.isEmpty())
        return;
    const int idx = indexOf(m_editingId);
    if (idx >= 0) {
        m_boxes[idx].html = m_boxEditor->toHtml();
        m_docs.remove(m_editingId);
        save();
    }
    m_editingId.clear();
    m_boxEditor->hide();
}

void PagedFloatingBoxes::closeEditor()
{
    if (!m_boxEditor)
        return;
    m_editingId.clear();
    m_boxEditor->hide();
}

void PagedFloatingBoxes::clearSelection()
{
    m_selectedId.clear();
}

bool PagedFloatingBoxes::editorVisible() const
{
    return m_boxEditor && m_boxEditor->isVisible();
}

bool PagedFloatingBoxes::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_boxEditor) {
        if (event->type() == QEvent::FocusOut) {
            commitEditor();
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                closeEditor();
                m_host->setFocus();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return
                && (keyEvent->modifiers() & Qt::ControlModifier)) {
                commitEditor();
                m_host->setFocus();
                return true;
            }
        }
    }
    return QObject::eventFilter(watched, event);
}
