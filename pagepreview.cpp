#include "pagepreview.h"
#include "pagedocumentpainter.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QTextDocument>
#include <QVBoxLayout>

#include <QtMath>

PagePreviewCanvas::PagePreviewCanvas(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Dark);
    setAutoFillBackground(true);
}

void PagePreviewCanvas::setDocument(QTextDocument *document)
{
    m_document = document;
    recalculate();
}

void PagePreviewCanvas::setHeaderFooter(const HeaderFooterSettings &settings)
{
    m_headerFooter = settings;
    update();
}

void PagePreviewCanvas::setPageLayout(const PageLayoutSettings &layout)
{
    m_layout = layout;
    recalculate();
}

void PagePreviewCanvas::setZoomPercent(int percent)
{
    m_zoom = qMax(0.25, percent / 100.0);
    recalculate();
}

void PagePreviewCanvas::setAvailableWidth(int widthPx)
{
    if (m_availableWidthPx == widthPx)
        return;
    m_availableWidthPx = qMax(0, widthPx);
    recalculate();
}

int PagePreviewCanvas::computeColumnCount() const
{
    if (!m_document || m_pageCount <= 1)
        return 1;

    const QSizeF page = PageDocumentPainter::pageSizePoints(m_layout);
    const qreal pageW = page.width() * m_zoom;
    const qreal gap = m_pageGap * m_zoom;
    const qreal side = m_sideMargin * m_zoom;
    const qreal avail = m_availableWidthPx > 0 ? m_availableWidthPx : pageW + 2 * side;

    // side + cols*pageW + (cols-1)*gap + side <= avail
    const qreal budget = avail - 2 * side + gap;
    if (budget < pageW)
        return 1;

    int cols = qFloor(budget / (pageW + gap));
    cols = qBound(1, cols, 3);
    return qMin(cols, m_pageCount);
}

void PagePreviewCanvas::recalculate()
{
    if (!m_document) {
        m_pageCount = 1;
        m_columnCount = 1;
        updateGeometry();
        update();
        return;
    }

    m_pageCount = PageDocumentPainter::pageCount(m_document, m_layout);
    m_columnCount = computeColumnCount();
    const int rows = qMax(1, (m_pageCount + m_columnCount - 1) / m_columnCount);

    const QSizeF page = PageDocumentPainter::pageSizePoints(m_layout);
    const int width = static_cast<int>(
        (2 * m_sideMargin + m_columnCount * page.width()
         + (m_columnCount - 1) * m_pageGap)
        * m_zoom);
    const int height = static_cast<int>(
        ((rows + 1) * m_pageGap + rows * page.height()) * m_zoom);
    setMinimumSize(width, height);
    resize(width, height);
    update();
}

void PagePreviewCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), QColor(90, 90, 90));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!m_document)
        return;

    const QSizeF page = PageDocumentPainter::pageSizePoints(m_layout);
    const auto result = PageDocumentPainter::layoutDocument(m_document, m_layout);
    painter.scale(m_zoom, m_zoom);

    const int cols = qMax(1, m_columnCount);
    for (int pageIndex = 0; pageIndex < m_pageCount; ++pageIndex) {
        const int col = pageIndex % cols;
        const int row = pageIndex / cols;
        const qreal x = m_sideMargin + col * (page.width() + m_pageGap);
        const qreal y = m_pageGap + row * (page.height() + m_pageGap);
        painter.save();
        painter.translate(x, y);
        PageDocumentPainter::paintLaidOutPage(&painter, pageIndex, result, m_layout,
                                              m_headerFooter, m_pageCount);
        painter.restore();
    }
}

PagePreviewDialog::PagePreviewDialog(QTextDocument *document,
                                     const HeaderFooterSettings &headerFooter,
                                     const PageLayoutSettings &layout,
                                     QWidget *parent)
    : QDialog(parent)
    , m_layout(layout)
{
    setWindowTitle(tr("分页预览"));
    resize(1000, 740);

    auto *previewDoc = document->clone(this);

    m_canvas = new PagePreviewCanvas(this);
    m_canvas->setHeaderFooter(headerFooter);
    m_canvas->setPageLayout(layout);
    m_canvas->setDocument(previewDoc);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_scroll->setWidget(m_canvas);

    auto *zoomLabel = new QLabel(tr("缩放:"), this);
    auto *zoomBox = new QComboBox(this);
    const QList<int> zooms = {50, 75, 100, 125, 150, 200};
    for (int z : zooms)
        zoomBox->addItem(QStringLiteral("%1%").arg(z), z);
    zoomBox->setCurrentIndex(2);

    m_pageLabel = new QLabel(this);
    auto refreshLabel = [this]() {
        const QSizeF size = m_layout.pageSizeMm();
        const int cols = m_canvas->columnCount();
        QString colsNote;
        if (cols > 1)
            colsNote = tr(" · %1 栏").arg(cols);
        m_pageLabel->setText(tr("%1 · %2×%3 mm · 共 %4 页%5")
                                 .arg(m_layout.paperName())
                                 .arg(size.width(), 0, 'f', 0)
                                 .arg(size.height(), 0, 'f', 0)
                                 .arg(m_canvas->pageCount())
                                 .arg(colsNote));
    };
    refreshLabel();

    connect(zoomBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, zoomBox, refreshLabel](int) {
                m_canvas->setZoomPercent(zoomBox->currentData().toInt());
                syncAvailableWidth();
                refreshLabel();
            });

    auto *top = new QHBoxLayout;
    top->addWidget(zoomLabel);
    top->addWidget(zoomBox);
    top->addSpacing(16);
    top->addWidget(m_pageLabel);
    top->addStretch();

    auto *root = new QVBoxLayout(this);
    root->addLayout(top);
    root->addWidget(m_scroll, 1);
}

void PagePreviewDialog::syncAvailableWidth()
{
    if (!m_scroll || !m_canvas)
        return;
    m_canvas->setAvailableWidth(m_scroll->viewport()->width());
    if (m_pageLabel) {
        const QSizeF size = m_layout.pageSizeMm();
        const int cols = m_canvas->columnCount();
        QString colsNote;
        if (cols > 1)
            colsNote = tr(" · %1 栏").arg(cols);
        m_pageLabel->setText(tr("%1 · %2×%3 mm · 共 %4 页%5")
                                 .arg(m_layout.paperName())
                                 .arg(size.width(), 0, 'f', 0)
                                 .arg(size.height(), 0, 'f', 0)
                                 .arg(m_canvas->pageCount())
                                 .arg(colsNote));
    }
}

void PagePreviewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    syncAvailableWidth();
}

void PagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    syncAvailableWidth();
}
