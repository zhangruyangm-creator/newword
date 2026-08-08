#include "floatingtextboxlayer.h"
#include "pagegeometry.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
constexpr qreal kPtToPx96 = 96.0 / 72.0;
constexpr int kResizeGrip = 8;
}

FloatingTextBoxLayer::FloatingTextBoxLayer(QWidget *parent)
    : QWidget(parent)
{
    // Cover the paper for coordinate math, but never steal clicks from the editor.
    // Interactive boxes are parented to pageFrame (see insert/reload), not to this layer.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(false);
}

void FloatingTextBoxLayer::setDocument(QTextDocument *document)
{
    m_document = document;
    reloadFromDocument();
}

void FloatingTextBoxLayer::setPageMetrics(int pageBodyHeightPx, int contentLeftPx, int contentTopPx,
                                          qreal zoomFactor)
{
    m_pageBodyHeightPx = pageBodyHeightPx;
    m_contentLeftPx = contentLeftPx;
    m_contentTopPx = contentTopPx;
    m_zoomFactor = qMax(0.25, zoomFactor);
    relayoutItems();
}

void FloatingTextBoxLayer::setVisibleInPageMode(bool visible)
{
    m_pageModeVisible = visible;
    setVisible(visible && m_document);
    for (FloatingTextBoxItem *item : m_items)
        item->setVisible(visible && m_document);
    if (visible)
        relayoutItems();
}

void FloatingTextBoxLayer::clearItems()
{
    for (FloatingTextBoxItem *item : m_items)
        item->deleteLater();
    m_items.clear();
}

void FloatingTextBoxLayer::reloadFromDocument()
{
    clearItems();
    if (!m_document)
        return;
    const QVector<FloatingTextBox> boxes = FloatingTextBoxes::load(m_document);
    QWidget *host = parentWidget() ? parentWidget() : this;
    for (const FloatingTextBox &box : boxes) {
        auto *item = new FloatingTextBoxItem(box, host);
        connect(item, &FloatingTextBoxItem::changed, this, [this, item]() {
            commitItemGeometry(item);
            syncToDocument();
            emit boxesChanged();
        });
        connect(item, &FloatingTextBoxItem::removeRequested, this, &FloatingTextBoxLayer::removeBox);
        connect(item, &FloatingTextBoxItem::selected, this, [this](const QString &id) {
            for (FloatingTextBoxItem *it : m_items) {
                const bool on = it->data().id == id;
                if (!on)
                    it->setEditing(false);
                it->setSelected(on);
            }
        });
        item->show();
        item->raise();
        m_items.append(item);
    }
    relayoutItems();
}

void FloatingTextBoxLayer::syncToDocument()
{
    if (!m_document)
        return;
    QVector<FloatingTextBox> boxes;
    boxes.reserve(m_items.size());
    for (FloatingTextBoxItem *item : m_items)
        boxes.append(item->data());
    FloatingTextBoxes::save(m_document, boxes, true);
}

FloatingTextBoxItem *FloatingTextBoxLayer::insertBox(int pageIndex)
{
    if (!m_document)
        return nullptr;
    FloatingTextBox box = FloatingTextBoxes::makeDefault(pageIndex);
    QWidget *host = parentWidget() ? parentWidget() : this;
    auto *item = new FloatingTextBoxItem(box, host);
    connect(item, &FloatingTextBoxItem::changed, this, [this, item]() {
        commitItemGeometry(item);
        syncToDocument();
        emit boxesChanged();
    });
    connect(item, &FloatingTextBoxItem::removeRequested, this, &FloatingTextBoxLayer::removeBox);
    connect(item, &FloatingTextBoxItem::selected, this, [this](const QString &id) {
        for (FloatingTextBoxItem *it : m_items) {
            const bool on = it->data().id == id;
            if (!on)
                it->setEditing(false);
            it->setSelected(on);
        }
    });
    item->show();
    item->raise();
    m_items.append(item);
    relayoutItems();
    syncToDocument();
    item->setSelected(true);
    item->setEditing(true);
    item->focusEditor();
    emit boxesChanged();
    return item;
}

void FloatingTextBoxLayer::removeBox(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i)->data().id != id)
            continue;
        m_items.at(i)->deleteLater();
        m_items.removeAt(i);
        syncToDocument();
        emit boxesChanged();
        return;
    }
}

void FloatingTextBoxLayer::clearSelection()
{
    for (FloatingTextBoxItem *it : m_items) {
        it->setEditing(false);
        it->setSelected(false);
    }
}

QRectF FloatingTextBoxLayer::itemGeometryPx(const FloatingTextBox &box) const
{
    const qreal scale = kPtToPx96 * m_zoomFactor;
    const qreal x = m_contentLeftPx + box.xPt * scale;
    const qreal y = m_contentTopPx + box.pageIndex * qMax(1, m_pageBodyHeightPx) + box.yPt * scale;
    const qreal w = box.wPt * scale;
    const qreal h = box.hPt * scale;
    return QRectF(x, y, w, h);
}

void FloatingTextBoxLayer::commitItemGeometry(FloatingTextBoxItem *item)
{
    if (!item || m_applyingLayout || m_pageBodyHeightPx <= 0 || m_zoomFactor < 0.01)
        return;
    const qreal scale = kPtToPx96 * m_zoomFactor;
    if (scale < 0.01)
        return;
    const QRect g = item->geometry();
    FloatingTextBox box = item->data();
    box.xPt = (g.x() - m_contentLeftPx) / scale;
    const int localY = g.y() - m_contentTopPx;
    box.pageIndex = qMax(0, localY / m_pageBodyHeightPx);
    box.yPt = (localY - box.pageIndex * m_pageBodyHeightPx) / scale;
    box.wPt = qMax(40.0, g.width() / scale);
    box.hPt = qMax(30.0, g.height() / scale);
    item->setData(box);
}

void FloatingTextBoxLayer::relayoutItems()
{
    if (!m_pageModeVisible || m_pageBodyHeightPx <= 0)
        return;
    m_applyingLayout = true;
    for (FloatingTextBoxItem *item : m_items) {
        item->applyGeometryPx(itemGeometryPx(item->data()));
        item->raise();
    }
    m_applyingLayout = false;
}

bool FloatingTextBoxLayer::event(QEvent *event)
{
    // Layer is WA_TransparentForMouseEvents; keep default handling for non-mouse events.
    return QWidget::event(event);
}

bool FloatingTextBoxLayer::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    Q_UNUSED(event)
    return QWidget::eventFilter(watched, event);
}

void FloatingTextBoxLayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutItems();
}

// --- FloatingTextBoxItem ---

FloatingTextBoxItem::FloatingTextBoxItem(const FloatingTextBox &data, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumSize(80, 40);
    setCursor(Qt::SizeAllCursor);

    m_editor = new QTextEdit(this);
    m_editor->setAcceptRichText(true);
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_editor->setStyleSheet(QStringLiteral(
        "QTextEdit { background: transparent; border: none; padding: 4px; }"));
    m_editor->setHtml(m_data.html.isEmpty() ? QStringLiteral("<p></p>") : m_data.html);
    // Default: clicks go to the frame for select/drag; enabled only while editing.
    m_editor->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_editor->setFocusPolicy(Qt::NoFocus);
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        m_data.html = m_editor->toHtml();
        emit changed();
    });
    m_editor->installEventFilter(this);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_editor, 1);

    updateChrome();
}

FloatingTextBox FloatingTextBoxItem::data() const
{
    FloatingTextBox box = m_data;
    box.html = m_editor ? m_editor->toHtml() : m_data.html;
    return box;
}

void FloatingTextBoxItem::setData(const FloatingTextBox &data)
{
    m_data = data;
    if (m_editor && m_editor->toHtml() != data.html) {
        const QSignalBlocker blocker(m_editor);
        m_editor->setHtml(data.html);
    }
}

void FloatingTextBoxItem::applyGeometryPx(const QRectF &rect)
{
    setGeometry(rect.toRect());
}

void FloatingTextBoxItem::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    if (!selected)
        setEditing(false);
    updateChrome();
    update();
}

void FloatingTextBoxItem::setEditing(bool editing)
{
    if (m_editing == editing)
        return;
    m_editing = editing;
    if (!m_editor)
        return;
    m_editor->setAttribute(Qt::WA_TransparentForMouseEvents, !editing);
    m_editor->setFocusPolicy(editing ? Qt::StrongFocus : Qt::NoFocus);
    if (editing) {
        m_selected = true;
        m_editor->setFocus(Qt::OtherFocusReason);
        setCursor(Qt::IBeamCursor);
    } else {
        if (m_editor->hasFocus())
            clearFocus();
        setCursor(Qt::SizeAllCursor);
    }
    updateChrome();
    update();
}

void FloatingTextBoxItem::focusEditor()
{
    setEditing(true);
    if (m_editor)
        m_editor->selectAll();
}

void FloatingTextBoxItem::updateChrome()
{
    // Border is painted in paintEvent (stylesheet dashed/rgba is unreliable on macOS).
    setStyleSheet(QStringLiteral(
        "FloatingTextBoxItem { background: rgba(255,255,255,0.92); border: none; }"));
}

bool FloatingTextBoxItem::nearResizeGrip(const QPoint &pos) const
{
    constexpr int kHit = 14;
    return QRect(width() - kHit, height() - kHit, kHit, kHit).contains(pos);
}

void FloatingTextBoxItem::beginMove(const QPoint &globalPos)
{
    m_drag = DragMode::Move;
    m_dragOrigin = globalPos;
    m_geomOrigin = geometry();
    m_movedEnough = false;
}

void FloatingTextBoxItem::beginResize(const QPoint &globalPos)
{
    m_drag = DragMode::Resize;
    m_dragOrigin = globalPos;
    m_geomOrigin = geometry();
    m_movedEnough = false;
}

void FloatingTextBoxItem::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    // Soft gray outline — never theme accent blue.
    const QColor border = m_selected ? QColor(160, 166, 176) : QColor(198, 202, 210);
    QPen pen(border, 1.0);
    pen.setCosmetic(true);
    pen.setStyle(m_selected ? Qt::SolidLine : Qt::DashLine);
    if (!m_selected)
        pen.setDashPattern({3.0, 2.5});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0));

    if (m_selected) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(150, 156, 168));
        const int s = 6;
        p.drawRect(width() - s - 1, height() - s - 1, s, s);
    }
}

void FloatingTextBoxItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    emit selected(m_data.id);
    setSelected(true);

    if (nearResizeGrip(event->pos())) {
        beginResize(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    // Not editing: drag to move. Editing: let editor handle (transparent off).
    if (!m_editing) {
        beginMove(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingTextBoxItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit selected(m_data.id);
        setSelected(true);
        setEditing(true);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void FloatingTextBoxItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == DragMode::Resize && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragOrigin;
        if (delta.manhattanLength() > 2)
            m_movedEnough = true;
        QRect g = m_geomOrigin;
        g.setWidth(qMax(80, m_geomOrigin.width() + delta.x()));
        g.setHeight(qMax(40, m_geomOrigin.height() + delta.y()));
        setGeometry(g);
        event->accept();
        return;
    }
    if (m_drag == DragMode::Move && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragOrigin;
        if (delta.manhattanLength() > 2)
            m_movedEnough = true;
        move(m_geomOrigin.topLeft() + delta);
        event->accept();
        return;
    }

    if (nearResizeGrip(event->pos()))
        setCursor(Qt::SizeFDiagCursor);
    else if (m_editing)
        setCursor(Qt::IBeamCursor);
    else
        setCursor(Qt::SizeAllCursor);
    QWidget::mouseMoveEvent(event);
}

void FloatingTextBoxItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drag != DragMode::None) {
        const bool wasMove = (m_drag == DragMode::Move);
        m_drag = DragMode::None;
        if (m_movedEnough)
            emit changed();
        else if (wasMove && m_selected && !m_editing) {
            // Click without drag → enter edit.
            setEditing(true);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingTextBoxItem::contextMenuEvent(QContextMenuEvent *event)
{
    emit selected(m_data.id);
    setSelected(true);
    QMenu menu(this);
    QAction *edit = menu.addAction(tr("编辑文字"));
    QAction *del = menu.addAction(tr("删除文本框"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == edit)
        setEditing(true);
    else if (chosen == del)
        emit removeRequested(m_data.id);
}

bool FloatingTextBoxItem::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor) {
        if (event->type() == QEvent::FocusIn) {
            emit selected(m_data.id);
            m_selected = true;
            m_editing = true;
            m_editor->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            updateChrome();
        }
        if (event->type() == QEvent::FocusOut) {
            // Keep selected; leave editing when focus leaves the box.
            setEditing(false);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                setEditing(false);
                setSelected(true);
                setFocus(Qt::OtherFocusReason);
                return true;
            }
            if ((ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace)
                && m_editor->toPlainText().trimmed().isEmpty() && !m_editing) {
                emit removeRequested(m_data.id);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
