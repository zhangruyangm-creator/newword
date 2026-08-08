#ifndef PAGEPREVIEW_H
#define PAGEPREVIEW_H

#include "headerfootersettings.h"
#include "pagelayout.h"

#include <QDialog>
#include <QWidget>

class QTextDocument;
class QScrollArea;
class QLabel;
class QShowEvent;
class QResizeEvent;

class PagePreviewCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PagePreviewCanvas(QWidget *parent = nullptr);

    void setDocument(QTextDocument *document);
    void setHeaderFooter(const HeaderFooterSettings &settings);
    void setPageLayout(const PageLayoutSettings &layout);
    void setZoomPercent(int percent);
    //! Viewport width used to decide 1 / 2 / 3 columns.
    void setAvailableWidth(int widthPx);

    int pageCount() const { return m_pageCount; }
    int columnCount() const { return m_columnCount; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void recalculate();
    [[nodiscard]] int computeColumnCount() const;

    QTextDocument *m_document = nullptr;
    HeaderFooterSettings m_headerFooter;
    PageLayoutSettings m_layout;
    qreal m_zoom = 1.0;
    int m_pageCount = 1;
    int m_columnCount = 1;
    int m_availableWidthPx = 0;
    qreal m_pageGap = 24.0;
    qreal m_sideMargin = 24.0;
};

class PagePreviewDialog : public QDialog
{
    Q_OBJECT

public:
    PagePreviewDialog(QTextDocument *document,
                      const HeaderFooterSettings &headerFooter,
                      const PageLayoutSettings &layout,
                      QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void syncAvailableWidth();

    PagePreviewCanvas *m_canvas = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_pageLabel = nullptr;
    PageLayoutSettings m_layout;
};

#endif // PAGEPREVIEW_H
