#ifndef FLOATINGTEXTBOXLAYER_H
#define FLOATINGTEXTBOXLAYER_H

#include "floatingtextbox.h"

#include <QVector>
#include <QWidget>

class QTextDocument;
class FloatingTextBoxItem;

/** Transparent overlay on the paper frame; hosts draggable floating text boxes. */
class FloatingTextBoxLayer : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingTextBoxLayer(QWidget *parent = nullptr);

    void setDocument(QTextDocument *document);
    void setPageMetrics(int pageBodyHeightPx, int contentLeftPx, int contentTopPx, qreal zoomFactor);
    void setVisibleInPageMode(bool visible);

    void reloadFromDocument();
    void syncToDocument();
    [[nodiscard]] FloatingTextBoxItem *insertBox(int pageIndex);
    void removeBox(const QString &id);
    void clearSelection();

signals:
    void boxesChanged();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void clearItems();
    void relayoutItems();
    void commitItemGeometry(FloatingTextBoxItem *item);
    [[nodiscard]] QRectF itemGeometryPx(const FloatingTextBox &box) const;

    QTextDocument *m_document = nullptr;
    QVector<FloatingTextBoxItem *> m_items;
    int m_pageBodyHeightPx = 0;
    int m_contentLeftPx = 0;
    int m_contentTopPx = 0;
    qreal m_zoomFactor = 1.0;
    bool m_pageModeVisible = true;
    bool m_applyingLayout = false;
};

/**
 * Interaction:
 * - Click → select (soft gray solid); drag → move; corner → resize
 * - Unselected → light gray dashed outline
 * - Double-click → edit text; Esc → leave edit, stay selected
 */
class FloatingTextBoxItem : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingTextBoxItem(const FloatingTextBox &data, QWidget *parent = nullptr);

    [[nodiscard]] FloatingTextBox data() const;
    void setData(const FloatingTextBox &data);
    void applyGeometryPx(const QRectF &rect);
    void setSelected(bool selected);
    [[nodiscard]] bool isSelected() const { return m_selected; }
    void setEditing(bool editing);
    [[nodiscard]] bool isEditing() const { return m_editing; }
    void focusEditor();

signals:
    void changed();
    void removeRequested(const QString &id);
    void selected(const QString &id);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class DragMode { None, Move, Resize };

    void updateChrome();
    [[nodiscard]] bool nearResizeGrip(const QPoint &pos) const;
    void beginMove(const QPoint &globalPos);
    void beginResize(const QPoint &globalPos);

    FloatingTextBox m_data;
    class QTextEdit *m_editor = nullptr;
    bool m_selected = false;
    bool m_editing = false;
    DragMode m_drag = DragMode::None;
    QPoint m_dragOrigin;
    QRect m_geomOrigin;
    bool m_movedEnough = false;
};

#endif // FLOATINGTEXTBOXLAYER_H
