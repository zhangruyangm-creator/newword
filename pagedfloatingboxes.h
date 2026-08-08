#ifndef PAGEDFLOATINGBOXES_H
#define PAGEDFLOATINGBOXES_H

#include "floatingtextbox.h"

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QVector>

#include <functional>

class QPainter;
class QPalette;
class QTextDocument;
class QTextEdit;
class QWidget;

//! Floating text boxes for the self-drawn paged editor: data, page-relative
//! geometry, painting, drag/resize and inline text editing.
class PagedFloatingBoxes : public QObject
{
    Q_OBJECT

public:
    explicit PagedFloatingBoxes(QWidget *host, QObject *parent = nullptr);

    void setDocument(QTextDocument *document) { m_document = document; }
    //! Provides the content-box rect (widget coords) for a page index.
    void setPageContentRect(const std::function<QRectF(int)> &provider)
    {
        m_pageContentRect = provider;
    }

    void reload();
    void insert(const FloatingTextBox &box);
    void remove(const QString &id);
    void selectAt(int index);
    bool openEditorAt(int index, qreal zoom);
    void paint(QPainter *painter, int pageIndex, qreal zoom, const QPalette &palette);
    [[nodiscard]] int hitTest(const QPoint &widgetPos, qreal zoom) const;
    //! Cursor shape over a box (resize grip / move); false when not on a box.
    [[nodiscard]] bool hoverShape(const QPoint &widgetPos, qreal zoom,
                                  Qt::CursorShape *shape) const;

    //! Drag interaction; returns false when no drag was active.
    bool beginDrag(int index, const QPoint &widgetPos, qreal zoom);
    bool dragTo(const QPoint &widgetPos, qreal zoom);
    bool endDrag(const QPoint &widgetPos, qreal zoom);
    [[nodiscard]] bool isDragging() const { return m_dragMove || m_dragResize; }

    bool openEditor(const QString &id, qreal zoom);
    void commitEditor();
    void closeEditor();
    [[nodiscard]] bool editorVisible() const;
    [[nodiscard]] QString selectedId() const { return m_selectedId; }
    void clearSelection();

signals:
    void boxesChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] int indexOf(const QString &id) const;
    [[nodiscard]] QRectF rectInWidget(const FloatingTextBox &box, qreal zoom) const;
    void save();

    QWidget *m_host = nullptr;
    QTextDocument *m_document = nullptr;
    std::function<QRectF(int)> m_pageContentRect;
    QVector<FloatingTextBox> m_boxes;
    QHash<QString, QTextDocument *> m_docs;
    QString m_selectedId;
    QString m_editingId;
    QTextEdit *m_boxEditor = nullptr;
    FloatingTextBox m_dragOrig;
    QPoint m_dragStart;
    bool m_dragMove = false;
    bool m_dragResize = false;
};

#endif // PAGEDFLOATINGBOXES_H
