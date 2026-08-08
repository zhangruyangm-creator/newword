#include "pagededitorwidget.h"

#include "appstyle.h"
#include "pagegeometry.h"
#include "tablegeometry.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QFileInfo>
#include <QInputMethodEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QCache>
#include <QPainter>
#include <QPixmap>
#include <QPalette>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextObjectInterface>
#include <QTextLayout>
#include <QTextLine>
#include <QTextTable>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr qreal kMmToPx = 96.0 / 25.4;
constexpr int kScrollbarWidth = 12;
constexpr int kEdgeAutoScrollPx = 24;

QTextCharFormat preeditFormat(const QPalette &palette)
{
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    fmt.setUnderlineColor(palette.color(QPalette::Text));
    return fmt;
}

bool isImagePath(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    static const QStringList kImageExts = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
        QStringLiteral("bmp"),  QStringLiteral("gif"),  QStringLiteral("webp"),
        QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("svg"),
        QStringLiteral("heic"), QStringLiteral("heif"),
    };
    return kImageExts.contains(ext);
}

bool dragCarriesImage(const QDropEvent *event)
{
    if (!event->mimeData() || !event->mimeData()->hasUrls())
        return false;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile() && isImagePath(url.toLocalFile()))
            return true;
    }
    return false;
}
} // namespace

//! Image object handler with a scaled-pixmap cache: each image is scaled once
//! per target size, then blitted — large-photo documents repaint cheaply.
class PagedImageObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    explicit PagedImageObject(QObject *parent = nullptr)
        : QObject(parent)
        , m_cache(2048) // ~128 MB of scaled pixmaps
    {
    }

    QSizeF intrinsicSize(QTextDocument *document, int, const QTextFormat &format) override
    {
        const QTextImageFormat imageFormat = format.toImageFormat();
        const QSizeF fmtSize(imageFormat.width(), imageFormat.height());
        const QImage image = qvariant_cast<QImage>(
            document->resource(QTextDocument::ImageResource, imageFormat.name()));
        if (fmtSize.width() > 0 && fmtSize.height() > 0)
            return fmtSize;
        if (image.isNull())
            return fmtSize;
        QSizeF size = image.size();
        if (fmtSize.width() > 0) {
            size.setHeight(size.height() * fmtSize.width() / size.width());
            size.setWidth(fmtSize.width());
        } else if (fmtSize.height() > 0) {
            size.setWidth(size.width() * fmtSize.height() / size.height());
            size.setHeight(fmtSize.height());
        }
        return size;
    }

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *document, int,
                    const QTextFormat &format) override
    {
        const QTextImageFormat imageFormat = format.toImageFormat();
        const QString name = imageFormat.name();
        if (name.isEmpty())
            return;
        const QImage image = qvariant_cast<QImage>(
            document->resource(QTextDocument::ImageResource, name));
        if (image.isNull())
            return;
        const QSize target = rect.size().toSize();
        if (target.width() <= 0 || target.height() <= 0)
            return;
        const QString key = QStringLiteral("%1@%2x%3").arg(name).arg(target.width())
                                .arg(target.height());
        QPixmap *cached = m_cache.object(key);
        if (!cached) {
            QPixmap pm = QPixmap::fromImage(image).scaled(
                target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (pm.isNull())
                return;
            m_cache.insert(key, new QPixmap(pm),
                           qMax(1, (pm.width() * pm.height() * 4) / 65536));
            cached = m_cache.object(key);
        }
        if (!cached)
            return;
        const QPointF offset((rect.width() - cached->width()) / 2.0,
                             (rect.height() - cached->height()) / 2.0);
        painter->drawPixmap(rect.topLeft() + offset, *cached);
    }

private:
    QCache<QString, QPixmap> m_cache;
};

PagedEditorWidget::PagedEditorWidget(QTextDocument *document,
                                     const PageLayoutSettings &layout,
                                     const HeaderFooterSettings &headerFooter,
                                     QWidget *parent)
    : QWidget(parent)
    , m_document(document)
    , m_layout(layout)
    , m_headerFooter(headerFooter)
    , m_cursor(m_document)
{
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(480, 360);

    m_vScroll = new QScrollBar(Qt::Vertical, this);
    m_vScroll->setFixedWidth(kScrollbarWidth);
    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int) {
        update();
        emit scrolled();
    });

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_cursorVisible = !m_cursorVisible;
        update();
    });

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(50);
    connect(m_autoScrollTimer, &QTimer::timeout, this, [this]() {
        const QPoint p = m_lastMousePos;
        if (p.y() < kEdgeAutoScrollPx) {
            scrollBy(-kEdgeAutoScrollPx * 2);
            setCursorFromWidget(p, true);
        } else if (p.y() > height() - kEdgeAutoScrollPx) {
            scrollBy(kEdgeAutoScrollPx * 2);
            setCursorFromWidget(p, true);
        }
    });

    if (m_document) {
        m_imageObject = new PagedImageObject(this);
        m_document->documentLayout()->registerHandler(QTextFormat::ImageObject, m_imageObject);
        connect(m_document, &QTextDocument::contentsChange, this,
                [this](int position, int charsRemoved, int charsAdded) {
                    // Qt can emit (0,0) events inside ordinary text edits, so a
                    // formatting-only burst is recognized only when NO chars
                    // changed in the whole edit.
                    if (charsRemoved != 0 || charsAdded != 0) {
                        if (m_formatCheckTimer)
                            m_formatCheckTimer->stop();
                        m_lastChangePos = position;
                        m_burstHasChars = true;
                    } else {
                        if (m_lastChangePos < 0)
                            m_lastChangePos = position;
                        m_burstHasFormatOnly = true;
                        if (m_formatCheckTimer)
                            m_formatCheckTimer->start();
                    }
                });
        connect(m_document, &QTextDocument::undoAvailable, this,
                &PagedEditorWidget::undoAvailable);
        connect(m_document, &QTextDocument::redoAvailable, this,
                &PagedEditorWidget::redoAvailable);
        connect(m_document, &QTextDocument::contentsChanged, this,
                [this]() { afterDocumentChange(); });
        connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                this, [this]() {
                    if (m_recomputingPagination)
                        return;
                    updatePageCount();
                    updateScrollBar();
                    update();
                });
        connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::pageCountChanged,
                this, [this](int) {
                    if (m_recomputingPagination)
                        return;
                    updatePageCount();
                    updateScrollBar();
                    update();
                });
    }

    m_fullPassTimer = new QTimer(this);
    m_fullPassTimer->setSingleShot(true);
    m_fullPassTimer->setInterval(120);
    connect(m_fullPassTimer, &QTimer::timeout, this, [this]() {
        if (!m_pageMode)
            return;
        if (m_maxLineHeightDirty || m_rebuildBlock >= 0)
            recomputePagination();
        updateScrollBar();
        update();
        emit pageInfoChanged();
    });

    // A genuine formatting change (font size / spacing) must trigger a full
    // pagination pass, but Qt also reports (0,0) events inside ordinary text
    // edits. Wait a short window: if real characters follow, treat it as an edit.
    m_formatCheckTimer = new QTimer(this);
    m_formatCheckTimer->setSingleShot(true);
    m_formatCheckTimer->setInterval(40);
    connect(m_formatCheckTimer, &QTimer::timeout, this, [this]() {
        if (m_burstHasChars)
            return; // a text edit followed the format event — not a format change
        if (m_burstHasFormatOnly) {
            m_maxLineHeightDirty = true;
            recomputePagination();
            updateScrollBar();
            update();
            emit pageInfoChanged();
        }
        m_burstHasChars = false;
        m_burstHasFormatOnly = false;
        m_lastChangePos = -1;
    });

    updateMetrics();
    relayoutDocument();
    updateScrollBar();
}

PagedEditorWidget::~PagedEditorWidget() = default;

void PagedEditorWidget::setTextCursor(const QTextCursor &cursor)
{
    m_cursor = cursor;
    afterCursorMove();
}

qreal PagedEditorWidget::zoomScale() const
{
    return PageGeometry::zoomFactorFor(m_zoomPercent);
}

int PagedEditorWidget::scrollValue() const
{
    return m_vScroll ? m_vScroll->value() : 0;
}

int PagedEditorWidget::pageOffsetX() const
{
    const qreal w = m_pageWidth * zoomScale();
    return qMax(0, qRound((width() - w) / 2.0));
}

int PagedEditorWidget::currentPageIndex() const
{
    if (!m_document || m_contentHeight <= 0)
        return 0;
    const int pos = m_cursor.position();
    const QTextBlock block = m_cursor.block();
    const QTextLayout *layout = block.layout();
    const int rel = pos - block.position();
    const QTextLine line = layout->lineForTextPosition(rel);
    const QRectF blockRect = m_document->documentLayout()->blockBoundingRect(block);
    if (!blockRect.isValid())
        return 0;
    // A block may span pages; the caret's page is determined by its LINE.
    const qreal docTop = blockRect.top() + (line.isValid() ? line.y() : 0.0);
    int page = pageIndexForDocY(qMax(0.0, docTop));
    ensureRangesThroughPage(page);
    return pageIndexForDocY(qMax(0.0, docTop));
}

void PagedEditorWidget::undo()
{
    if (!m_document || !m_document->isUndoAvailable())
        return;
    const int pos = m_cursor.position();
    m_document->undo();
    m_cursor = QTextCursor(m_document);
    m_cursor.setPosition(qMin(pos, m_document->characterCount() - 1));
    afterCursorMove();
}

void PagedEditorWidget::redo()
{
    if (!m_document || !m_document->isRedoAvailable())
        return;
    const int pos = m_cursor.position();
    m_document->redo();
    m_cursor = QTextCursor(m_document);
    m_cursor.setPosition(qMin(pos, m_document->characterCount() - 1));
    afterCursorMove();
}

void PagedEditorWidget::cut()
{
    if (!m_cursor.hasSelection())
        return;
    copy();
    deleteChar(true);
}

void PagedEditorWidget::copy()
{
    if (!m_cursor.hasSelection())
        return;
    const QTextDocumentFragment frag(m_cursor);
    auto *mime = new QMimeData;
    mime->setHtml(frag.toHtml());
    mime->setText(frag.toPlainText());
    QApplication::clipboard()->setMimeData(mime);
}

void PagedEditorWidget::paste()
{
    if (!m_document || m_readOnly)
        return;
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime)
        return;
    if (m_cursor.hasSelection())
        m_cursor.removeSelectedText();
    if (mime->hasHtml() && m_acceptRichText) {
        m_cursor.insertHtml(mime->html());
    } else if (mime->hasText()) {
        m_cursor.insertText(mime->text());
    } else {
        return;
    }
    afterCursorMove();
}

void PagedEditorWidget::selectAll()
{
    m_cursor.select(QTextCursor::Document);
    afterCursorMove();
}

QString PagedEditorWidget::toPlainText() const
{
    return m_document ? m_document->toPlainText() : QString();
}

QString PagedEditorWidget::toHtml() const
{
    return m_document ? m_document->toHtml() : QString();
}

void PagedEditorWidget::setHtml(const QString &html)
{
    if (!m_document)
        return;
    m_document->setHtml(html);
    normalizeDocumentStructure(m_document);
    downscaleImageResources(m_document);
    m_currentCharFormat = QTextCharFormat();
    m_cursor = QTextCursor(m_document);
    recomputePagination(); // bulk replace: exact pagination right away
    updateScrollBar();
    update();
    afterCursorMove();
}

void PagedEditorWidget::normalizeDocumentStructure(QTextDocument *document)
{
    if (!document || document->characterCount() < 2)
        return;
    // Qt's HTML import can produce giant single paragraphs (or heavily
    // fragmented runs) whose later edits cost O(n) per keystroke. Rebuilding
    // via the cursor API fixes that. Well-formed multi-paragraph documents
    // edit fast already — skip the one-time rebuild for them.
    const int chars = document->characterCount();
    const int blockCount = document->blockCount();
    if (blockCount >= qMax(4, chars / 400))
        return;

    struct Run {
        bool isImage = false;
        QTextImageFormat imageFormat;
        QTextCharFormat charFormat;
        QString text;
    };
    struct BlockData {
        QTextBlockFormat blockFormat;
        QVector<Run> runs;
    };

    QVector<BlockData> blocks;
    for (QTextBlock b = document->begin(); b.isValid(); b = b.next()) {
        BlockData data;
        data.blockFormat = b.blockFormat();
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            Run run;
            run.charFormat = fragment.charFormat();
            run.text = fragment.text();
            if (run.charFormat.isImageFormat()) {
                run.isImage = true;
                run.imageFormat = run.charFormat.toImageFormat();
            }
            data.runs.append(run);
        }
        blocks.append(data);
    }

    const bool undoEnabled = document->isUndoRedoEnabled();
    document->setUndoRedoEnabled(false);
    QTextCursor c(document);
    c.select(QTextCursor::Document);
    c.removeSelectedText();
    c.setPosition(0);
    const int n = blocks.size();
    for (int i = 0; i < n; ++i) {
        if (i == 0) {
            c.setBlockFormat(blocks.at(i).blockFormat);
        } else {
            c.insertBlock(blocks.at(i).blockFormat);
        }
        for (const Run &run : blocks.at(i).runs) {
            if (run.isImage)
                c.insertImage(run.imageFormat);
            else
                c.insertText(run.text, run.charFormat);
        }
    }
    document->setUndoRedoEnabled(undoEnabled);
}

void PagedEditorWidget::downscaleImageResources(QTextDocument *document)
{
    if (!document)
        return;
    constexpr int kMaxImageDim = 2048; // enough for print at display sizes
    for (QTextBlock b = document->begin(); b.isValid(); b = b.next()) {
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.charFormat().isImageFormat())
                continue;
            const QString name = fragment.charFormat().toImageFormat().name();
            if (name.isEmpty())
                continue;
            const QVariant resource =
                document->resource(QTextDocument::ImageResource, name);
            if (!resource.canConvert<QImage>())
                continue;
            const QImage image = resource.value<QImage>();
            const int longest = qMax(image.width(), image.height());
            if (longest <= kMaxImageDim)
                continue;
            const QImage scaled = image.scaled(
                image.width() * kMaxImageDim / longest,
                image.height() * kMaxImageDim / longest, Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            document->addResource(QTextDocument::ImageResource, name, scaled);
        }
    }
}

void PagedEditorWidget::reloadFloatingTextBoxes()
{
    m_floatBoxes = FloatingTextBoxes::load(m_document);
    m_floatBoxDocs.clear();
    if (!m_selectedBoxId.isEmpty() && indexOfFloatingBox(m_selectedBoxId) < 0)
        m_selectedBoxId.clear();
    closeBoxEditor();
    update();
}

void PagedEditorWidget::insertFloatingTextBox(const FloatingTextBox &box)
{
    m_floatBoxes.append(box);
    saveFloatingBoxes();
    m_selectedBoxId = box.id;
    update();
}

void PagedEditorWidget::removeFloatingTextBox(const QString &id)
{
    const int idx = indexOfFloatingBox(id);
    if (idx < 0)
        return;
    closeBoxEditor();
    m_floatBoxes.removeAt(idx);
    m_floatBoxDocs.remove(id);
    if (m_selectedBoxId == id)
        m_selectedBoxId.clear();
    saveFloatingBoxes();
}

void PagedEditorWidget::saveFloatingBoxes()
{
    if (!m_document)
        return;
    FloatingTextBoxes::save(m_document, m_floatBoxes, true);
    update();
    emit floatingBoxesChanged();
}

int PagedEditorWidget::indexOfFloatingBox(const QString &id) const
{
    for (int i = 0; i < m_floatBoxes.size(); ++i) {
        if (m_floatBoxes.at(i).id == id)
            return i;
    }
    return -1;
}

QRectF PagedEditorWidget::floatBoxRectInWidget(const FloatingTextBox &box) const
{
    const qreal f = zoomScale();
    constexpr qreal kPtToPx = 96.0 / 72.0;
    const int page = qBound(0, box.pageIndex, qMax(0, m_pageCount - 1));
    const QRectF content = contentRectInWidget(page);
    return QRectF(content.left() + box.xPt * kPtToPx * f,
                  content.top() + box.yPt * kPtToPx * f,
                  box.wPt * kPtToPx * f,
                  box.hPt * kPtToPx * f);
}

int PagedEditorWidget::hitTestFloatingBox(const QPoint &widgetPos) const
{
    for (int i = m_floatBoxes.size() - 1; i >= 0; --i) {
        if (floatBoxRectInWidget(m_floatBoxes.at(i)).adjusted(-2, -2, 2, 2)
                .contains(widgetPos)) {
            return i;
        }
    }
    return -1;
}

void PagedEditorWidget::paintFloatingBoxes(QPainter *painter, int pageIndex)
{
    for (int i = 0; i < m_floatBoxes.size(); ++i) {
        const FloatingTextBox &box = m_floatBoxes.at(i);
        if (box.pageIndex != pageIndex)
            continue;
        const QRectF r = floatBoxRectInWidget(box);
        painter->fillRect(r, QColor(255, 255, 255, 238));

        const bool selected = box.id == m_selectedBoxId;
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
            QTextDocument *boxDoc = m_floatBoxDocs.value(box.id);
            if (!boxDoc) {
                boxDoc = new QTextDocument(this);
                boxDoc->setDocumentMargin(0);
                boxDoc->setHtml(box.html);
                m_floatBoxDocs.insert(box.id, boxDoc);
            }
            boxDoc->setPageSize(content.size());
            painter->save();
            painter->setClipRect(content);
            painter->translate(content.topLeft());
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.palette = palette();
            boxDoc->documentLayout()->draw(painter, ctx);
            painter->restore();
        }

        if (selected) {
            painter->fillRect(QRectF(r.right() - 7, r.bottom() - 7, 7, 7),
                              QColor(150, 156, 168));
        }
    }
}

void PagedEditorWidget::setGridLinesVisible(bool visible)
{
    if (m_showGrid == visible)
        return;
    m_showGrid = visible;
    update();
}

void PagedEditorWidget::paintGridLines(QPainter *painter, const QRectF &contentRect)
{
    if (!m_showGrid || contentRect.width() < 8 || contentRect.height() < 8)
        return;
    const qreal step = m_gridSpacingPx * zoomScale();
    painter->save();
    painter->setPen(QPen(QColor(190, 198, 210, 110), 1));
    for (qreal x = contentRect.left(); x <= contentRect.right(); x += step) {
        painter->drawLine(QPointF(x, contentRect.top()), QPointF(x, contentRect.bottom()));
    }
    for (qreal y = contentRect.top(); y <= contentRect.bottom(); y += step) {
        painter->drawLine(QPointF(contentRect.left(), y), QPointF(contentRect.right(), y));
    }
    painter->restore();
}

int PagedEditorWidget::hitTestColumnBorder(const QPoint &widgetPos, QTextTable **tableOut,
                                           QRectF *tableRectOut) const
{
    if (tableOut)
        *tableOut = nullptr;
    if (tableRectOut)
        *tableRectOut = {};
    if (!m_document || m_readOnly)
        return -1;

    QTextCursor cursor = cursorForPosition(widgetPos);
    QTextTable *table = cursor.currentTable();
    if (!table || table->columns() < 2)
        return -1;

    auto *layout = m_document->documentLayout();
    const QRectF tableRect = layout->frameBoundingRect(table);
    if (!tableRect.isValid() || tableRect.height() < 2.0)
        return -1;

    const QPointF docPos = documentPointAt(widgetPos);
    constexpr qreal kYPad = 2.0;
    if (docPos.y() < tableRect.top() - kYPad || docPos.y() > tableRect.bottom() + kYPad)
        return -1;

    const QVector<qreal> edges = TableGeometry::columnEdgeXs(table, tableRect);
    if (edges.size() < 3)
        return -1;

    // Tolerance in widget pixels, converted to doc units for the current zoom.
    const qreal kTolerance = 5.0 / qMax(0.25, zoomScale());
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

void PagedEditorWidget::updateColumnResizeCursor(const QPoint &widgetPos)
{
    if (m_columnResize.active)
        return;
    QTextTable *table = nullptr;
    const int border = hitTestColumnBorder(widgetPos, &table);
    const bool hover = border >= 0 && table;
    if (hover)
        setCursor(Qt::SplitHCursor);
    else if (m_hoveringColumnBorder)
        unsetCursor();
    m_hoveringColumnBorder = hover;
}

void PagedEditorWidget::applyColumnResizeDrag(const QPoint &widgetPos)
{
    if (!m_columnResize.active || !m_columnResize.table)
        return;
    if (m_columnResize.tableWidth < 8.0)
        return;

    const QPointF docPos = documentPointAt(widgetPos);
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
    update();
}

void PagedEditorWidget::openBoxEditor(const QString &id)
{
    if (m_readOnly)
        return;
    const int idx = indexOfFloatingBox(id);
    if (idx < 0)
        return;
    if (!m_boxEditor) {
        m_boxEditor = new QTextEdit(this);
        m_boxEditor->setFrameShape(QFrame::NoFrame);
        m_boxEditor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: white; border: 1px solid #8a93a3; }"));
        m_boxEditor->installEventFilter(this);
        m_boxEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_boxEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_boxEditor->hide();
    }
    m_editingBoxId = id;
    const FloatingTextBox &box = m_floatBoxes.at(idx);
    m_boxEditor->setGeometry(floatBoxRectInWidget(box).toRect().adjusted(1, 1, -1, -1));
    m_boxEditor->setHtml(box.html);
    m_boxEditor->show();
    m_boxEditor->raise();
    m_boxEditor->setFocus();
}

void PagedEditorWidget::commitBoxEditor()
{
    if (!m_boxEditor || m_editingBoxId.isEmpty())
        return;
    const int idx = indexOfFloatingBox(m_editingBoxId);
    if (idx >= 0) {
        m_floatBoxes[idx].html = m_boxEditor->toHtml();
        m_floatBoxDocs.remove(m_editingBoxId);
        saveFloatingBoxes();
    }
    m_editingBoxId.clear();
    m_boxEditor->hide();
}

void PagedEditorWidget::closeBoxEditor()
{
    if (!m_boxEditor)
        return;
    m_editingBoxId.clear();
    m_boxEditor->hide();
}

bool PagedEditorWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_boxEditor) {
        if (event->type() == QEvent::FocusOut) {
            commitBoxEditor();
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                closeBoxEditor();
                setFocus();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return
                && (keyEvent->modifiers() & Qt::ControlModifier)) {
                commitBoxEditor();
                setFocus();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PagedEditorWidget::setPlainText(const QString &text)
{
    if (!m_document)
        return;
    m_document->setPlainText(text);
    m_currentCharFormat = QTextCharFormat();
    m_cursor = QTextCursor(m_document);
    recomputePagination();
    updateScrollBar();
    update();
    afterCursorMove();
}

void PagedEditorWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    update();
}

void PagedEditorWidget::setUndoRedoEnabled(bool enabled)
{
    if (m_document)
        m_document->setUndoRedoEnabled(enabled);
}

void PagedEditorWidget::setTabStopDistance(qreal distance)
{
    if (!m_document)
        return;
    QTextOption opt = m_document->defaultTextOption();
    opt.setTabStopDistance(distance);
    m_document->setDefaultTextOption(opt);
}

QColor PagedEditorWidget::textColor() const
{
    const QColor c = currentCharFormat().foreground().color();
    return c.isValid() ? c : QColor(Qt::black);
}

QFont PagedEditorWidget::currentFont() const
{
    const QFont f = currentCharFormat().font();
    return f.family().isEmpty() && m_document ? m_document->defaultFont() : f;
}

QTextCharFormat PagedEditorWidget::currentCharFormat() const
{
    if (!m_currentCharFormat.isEmpty())
        return m_currentCharFormat;
    return m_cursor.charFormat();
}

void PagedEditorWidget::setCurrentCharFormat(const QTextCharFormat &format)
{
    if (m_cursor.hasSelection()) {
        m_cursor.mergeCharFormat(format);
        afterCursorMove();
    } else {
        m_currentCharFormat = format;
        emit currentCharFormatChanged(m_currentCharFormat);
    }
}

void PagedEditorWidget::mergeCurrentCharFormat(const QTextCharFormat &modifier)
{
    if (m_cursor.hasSelection()) {
        m_cursor.mergeCharFormat(modifier);
        afterCursorMove();
    } else {
        m_currentCharFormat = m_cursor.charFormat();
        m_currentCharFormat.merge(modifier);
        emit currentCharFormatChanged(m_currentCharFormat);
    }
}

Qt::Alignment PagedEditorWidget::alignment() const
{
    return m_cursor.blockFormat().alignment();
}

void PagedEditorWidget::setAlignment(Qt::Alignment alignment)
{
    if (!m_document)
        return;
    QTextBlockFormat fmt;
    fmt.setAlignment(alignment);
    QTextCursor c = m_cursor;
    if (c.hasSelection()) {
        c.beginEditBlock();
        const int end = c.selectionEnd();
        c.setPosition(c.selectionStart());
        while (c.position() <= end && !c.atEnd()) {
            c.mergeBlockFormat(fmt);
            if (!c.movePosition(QTextCursor::NextBlock))
                break;
            if (c.position() > end)
                break;
        }
        c.endEditBlock();
    } else {
        c.mergeBlockFormat(fmt);
    }
    m_cursor = c;
    afterCursorMove();
}

bool PagedEditorWidget::find(const QString &expression, QTextDocument::FindFlags flags)
{
    if (!m_document || expression.isEmpty())
        return false;
    QTextCursor found = m_document->find(expression, m_cursor, flags);
    if (found.isNull()) {
        if (flags & QTextDocument::FindBackward) {
            QTextCursor end(m_document);
            end.movePosition(QTextCursor::End);
            found = m_document->find(expression, end, flags);
        } else {
            QTextCursor start(m_document);
            found = m_document->find(expression, start, flags);
        }
    }
    if (found.isNull())
        return false;
    m_cursor = found;
    afterCursorMove();
    return true;
}

QTextCursor PagedEditorWidget::cursorForPosition(const QPoint &pos) const
{
    QTextCursor c(m_document);
    if (!m_document)
        return c;
    const QPointF docPoint = documentPointAt(pos);
    int hit = m_document->documentLayout()->hitTest(docPoint, Qt::FuzzyHit);
    if (hit < 0)
        hit = m_document->characterCount() - 1;
    c.setPosition(hit);
    return c;
}

QMenu *PagedEditorWidget::createStandardContextMenu()
{
    auto *menu = new QMenu(this);
    QAction *undoAction = menu->addAction(tr("撤销"));
    undoAction->setEnabled(m_document && m_document->isUndoAvailable());
    connect(undoAction, &QAction::triggered, this, &PagedEditorWidget::undo);
    QAction *redoAction = menu->addAction(tr("重做"));
    redoAction->setEnabled(m_document && m_document->isRedoAvailable());
    connect(redoAction, &QAction::triggered, this, &PagedEditorWidget::redo);
    menu->addSeparator();
    QAction *cutAction = menu->addAction(tr("剪切"));
    cutAction->setEnabled(!m_readOnly && m_cursor.hasSelection());
    connect(cutAction, &QAction::triggered, this, &PagedEditorWidget::cut);
    QAction *copyAction = menu->addAction(tr("复制"));
    copyAction->setEnabled(m_cursor.hasSelection());
    connect(copyAction, &QAction::triggered, this, &PagedEditorWidget::copy);
    QAction *pasteAction = menu->addAction(tr("粘贴"));
    pasteAction->setEnabled(!m_readOnly && QApplication::clipboard()->mimeData()
                            && !QApplication::clipboard()->mimeData()->text().isEmpty());
    connect(pasteAction, &QAction::triggered, this, &PagedEditorWidget::paste);
    menu->addSeparator();
    QAction *allAction = menu->addAction(tr("全选"));
    connect(allAction, &QAction::triggered, this, &PagedEditorWidget::selectAll);
    return menu;
}

QRectF PagedEditorWidget::cursorRect() const
{
    if (!m_document || m_contentHeight <= 0)
        return QRectF();
    const int pos = m_cursor.position();
    const QTextBlock block = m_cursor.block();
    const QTextLayout *layout = block.layout();
    const int rel = pos - block.position();
    const QTextLine line = layout->lineForTextPosition(rel);
    const QRectF blockRect = m_document->documentLayout()->blockBoundingRect(block);
    QRectF docRect;
    if (line.isValid()) {
        const qreal x = line.cursorToX(rel);
        // line.y() / cursorToX() are relative to the block; translate by the
        // block's position in document coordinates.
        // The caret is a thin vertical line — never cover the whole glyph.
        docRect = QRectF(blockRect.left() + x,
                         blockRect.top() + line.y(),
                         2.0, line.height());
    } else {
        docRect = QRectF(blockRect.left(), blockRect.top(), 2, blockRect.height());
    }
    int page = pageIndexForDocY(qMax(0.0, docRect.top()));
    ensureRangesThroughPage(page);
    page = pageIndexForDocY(qMax(0.0, docRect.top()));
    const QRectF content = contentRectInWidget(page);
    const qreal f = zoomScale();
    return QRectF(content.left() + docRect.left() * f,
                  content.top() + (docRect.top() - m_pageRanges.at(page).start) * f,
                  qMax(1.0, docRect.width() * f), docRect.height() * f);
}

void PagedEditorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), m_pageMode ? QColor(AppStyle::desk())
                                  : QColor(AppStyle::surface()));
    if (!m_document || m_pageCount <= 0)
        return;

    const int scrollY = m_vScroll->value();
    int first = 0;
    int last = 0;
    if (m_pageMode) {
        const qreal f = zoomScale();
        const qreal stride = (m_pageHeight + m_gap) * f;
        first = qMax(0, int(std::floor((scrollY - m_topPad * f) / stride)));
        last = qMin(m_pageCount - 1,
                    int(std::ceil((scrollY + height() - m_topPad * f) / stride)));
    }
    for (int k = first; k <= last; ++k)
        drawPage(&p, k);
}

void PagedEditorWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    m_vScroll->setGeometry(width() - kScrollbarWidth, 0, kScrollbarWidth, height());
    updateScrollBar();
    emit pageGeometryChanged();
}

void PagedEditorWidget::wheelEvent(QWheelEvent *event)
{
    const QPoint numPixels = event->pixelDelta();
    const QPoint numDegrees = event->angleDelta();
    int delta = 0;
    if (!numPixels.isNull()) {
        delta = numPixels.y();
    } else if (!numDegrees.isNull()) {
        delta = numDegrees.y() * height() / 240;
    }
    if (delta != 0) {
        scrollBy(-delta);
        event->accept();
    } else {
        event->ignore();
    }
}

void PagedEditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int boxIdx = hitTestFloatingBox(event->pos());
        if (boxIdx >= 0 && !m_readOnly) {
            m_selectedBoxId = m_floatBoxes.at(boxIdx).id;
            m_boxDragOrig = m_floatBoxes.at(boxIdx);
            m_boxDragStart = event->pos();
            const QRectF r = floatBoxRectInWidget(m_boxDragOrig);
            m_boxDragResize =
                QRectF(r.right() - 14, r.bottom() - 14, 14, 14).contains(event->pos());
            m_boxDragMove = !m_boxDragResize;
            m_dragging = false;
            update();
            event->accept();
            return;
        }
        QTextTable *table = nullptr;
        QRectF tableRect;
        const int border = hitTestColumnBorder(event->pos(), &table, &tableRect);
        if (border >= 0 && table) {
            m_columnResize.active = true;
            m_columnResize.table = table;
            m_columnResize.borderAfterColumn = border;
            m_columnResize.startPercents = TableGeometry::columnWidthPercents(table);
            m_columnResize.startDocX = documentPointAt(event->pos()).x();
            m_columnResize.tableWidth = tableRect.width();
            m_columnResize.guideDocX = m_columnResize.startDocX;
            setCursor(Qt::SplitHCursor);
            m_dragging = false;
            event->accept();
            return;
        }
        m_selectedBoxId.clear();
        update();
        m_hasSelectionDrag = true;
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursorFromWidget(event->pos(), false);
        event->accept();
    } else {
        event->ignore();
    }
}

void PagedEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (m_columnResize.active) {
        applyColumnResizeDrag(event->pos());
        event->accept();
        return;
    }
    if (m_boxDragMove || m_boxDragResize) {
        const int idx = indexOfFloatingBox(m_selectedBoxId);
        if (idx >= 0) {
            const qreal f = zoomScale();
            constexpr qreal kPtToPx = 96.0 / 72.0;
            const qreal inv = 1.0 / (kPtToPx * f);
            const QPointF delta = (event->pos() - m_boxDragStart) * inv;
            FloatingTextBox &box = m_floatBoxes[idx];
            if (m_boxDragMove) {
                box.xPt = qMax(0.0, m_boxDragOrig.xPt + delta.x());
                box.yPt = qMax(0.0, m_boxDragOrig.yPt + delta.y());
            } else {
                box.wPt = qMax(30.0, m_boxDragOrig.wPt + delta.x());
                box.hPt = qMax(20.0, m_boxDragOrig.hPt + delta.y());
            }
            update();
        }
        event->accept();
        return;
    }
    if (!m_dragging)
        updateColumnResizeCursor(event->pos());
    if (m_dragging && m_hasSelectionDrag) {
        setCursorFromWidget(event->pos(), true);
        const bool nearEdge = event->pos().y() < kEdgeAutoScrollPx
                              || event->pos().y() > height() - kEdgeAutoScrollPx;
        if (nearEdge)
            m_autoScrollTimer->start();
        else
            m_autoScrollTimer->stop();
        event->accept();
        return;
    }
    event->ignore();
}

void PagedEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_columnResize.active) {
            applyColumnResizeDrag(event->pos());
            m_columnResize = {};
            updateColumnResizeCursor(event->pos());
            event->accept();
            return;
        }
        if (m_boxDragMove || m_boxDragResize) {
            m_boxDragMove = false;
            m_boxDragResize = false;
            saveFloatingBoxes();
            event->accept();
            return;
        }
        m_dragging = false;
        m_autoScrollTimer->stop();
        event->accept();
    } else {
        event->ignore();
    }
}

void PagedEditorWidget::leaveEvent(QEvent *event)
{
    if (!m_columnResize.active && m_hoveringColumnBorder) {
        unsetCursor();
        m_hoveringColumnBorder = false;
    }
    QWidget::leaveEvent(event);
}

void PagedEditorWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int boxIdx = hitTestFloatingBox(event->pos());
    if (boxIdx >= 0) {
        m_selectedBoxId = m_floatBoxes.at(boxIdx).id;
        openBoxEditor(m_selectedBoxId);
        update();
        event->accept();
        return;
    }
    const QTextCursor clickCursor = cursorForPosition(event->pos());
    const int hitPos = clickCursor.position();
    auto isImageAt = [&](int p) -> bool {
        if (p < 0 || !m_document)
            return false;
        const QTextBlock b = m_document->findBlock(p);
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment f = it.fragment();
            if (f.charFormat().isImageFormat() && p >= f.position()
                && p < f.position() + f.length())
                return true;
        }
        return false;
    };
    if (isImageAt(hitPos) || isImageAt(hitPos - 1)) {
        emit imageDoubleClicked(hitPos);
        event->accept();
        return;
    }
    setCursorFromWidget(event->pos(), false);
    m_cursor.select(QTextCursor::WordUnderCursor);
    m_hasSelectionDrag = true;
    m_dragging = true;
    m_lastMousePos = event->pos();
    afterCursorMove();
    event->accept();
}

void PagedEditorWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_document) {
        event->ignore();
        return;
    }
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();
    const QString text = event->text();

    if (key == Qt::Key_Delete && !m_selectedBoxId.isEmpty()
        && (!m_boxEditor || !m_boxEditor->isVisible())) {
        removeFloatingTextBox(m_selectedBoxId);
        m_selectedBoxId.clear();
        event->accept();
        return;
    }

    if (m_readOnly) {
        const bool editing = key == Qt::Key_Backspace || key == Qt::Key_Delete
                             || key == Qt::Key_Return || key == Qt::Key_Enter
                             || key == Qt::Key_Tab || (!text.isEmpty() && text.at(0).isPrint())
                             || ((mods & Qt::ControlModifier)
                                 && (key == Qt::Key_Z || key == Qt::Key_Y || key == Qt::Key_X
                                     || key == Qt::Key_V));
        if (editing) {
            event->ignore();
            return;
        }
    }

    if (mods & Qt::ControlModifier) {
        switch (key) {
        case Qt::Key_Z:
            if (mods & Qt::ShiftModifier)
                redo();
            else
                undo();
            event->accept();
            return;
        case Qt::Key_Y:
            redo();
            event->accept();
            return;
        case Qt::Key_A:
            selectAll();
            event->accept();
            return;
        case Qt::Key_C:
            copy();
            event->accept();
            return;
        case Qt::Key_X:
            cut();
            event->accept();
            return;
        case Qt::Key_V:
            paste();
            event->accept();
            return;
        case Qt::Key_Home:
            m_cursor.movePosition(QTextCursor::Start);
            afterCursorMove();
            event->accept();
            return;
        case Qt::Key_End:
            m_cursor.movePosition(QTextCursor::End);
            afterCursorMove();
            event->accept();
            return;
        default:
            break;
        }
    }

    const bool wordMove = (mods & Qt::ControlModifier) || (mods & Qt::AltModifier);
    switch (key) {
    case Qt::Key_Left:
        moveCursor(wordMove ? QTextCursor::WordLeft : QTextCursor::Left,
                   mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                            : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_Right:
        moveCursor(wordMove ? QTextCursor::WordRight : QTextCursor::Right,
                   mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                            : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_Up:
        moveCursor(QTextCursor::Up, mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                             : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_Down:
        moveCursor(QTextCursor::Down, mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                               : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_Home:
        moveCursor(QTextCursor::StartOfLine,
                   mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                            : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_End:
        moveCursor(QTextCursor::EndOfLine,
                   mods & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                            : QTextCursor::MoveAnchor);
        event->accept();
        return;
    case Qt::Key_PageUp:
        scrollBy(-qMax(1, height() - 48));
        event->accept();
        return;
    case Qt::Key_PageDown:
        scrollBy(qMax(1, height() - 48));
        event->accept();
        return;
    case Qt::Key_Backspace:
        deleteChar(true);
        event->accept();
        return;
    case Qt::Key_Delete:
        deleteChar(false);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_cursor.hasSelection())
            m_cursor.removeSelectedText();
        m_cursor.insertBlock();
        afterCursorMove();
        event->accept();
        return;
    case Qt::Key_Tab:
        if (m_cursor.hasSelection())
            m_cursor.removeSelectedText();
        m_cursor.insertText(QStringLiteral("\t"));
        afterCursorMove();
        event->accept();
        return;
    default:
        break;
    }

    if ((mods & Qt::ControlModifier) || (mods & Qt::AltModifier))
        return; // unhandled shortcut-like key
    if (!text.isEmpty() && text.at(0).isPrint()) {
        insertText(text);
        event->accept();
        return;
    }
    event->ignore();
}

void PagedEditorWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_readOnly) {
        event->accept();
        return;
    }
    if (event->commitString().isEmpty() && event->preeditString().isEmpty()) {
        event->accept();
        return;
    }
    const bool composing = m_preeditLength > 0;
    removePreedit();
    if (!event->preeditString().isEmpty())
        insertPreedit(event->preeditString());
    if (!event->commitString().isEmpty()) {
        if (composing) {
            // Commit with the format captured before composition started, so the
            // IME underline never leaks into committed text.
            QTextCursor c = m_cursor;
            if (c.hasSelection())
                c.removeSelectedText();
            c.insertText(event->commitString(), m_preeditBaseFormat);
            m_cursor = c;
        } else {
            insertText(event->commitString());
        }
    }
    afterCursorMove();
    event->accept();
}

QVariant PagedEditorWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImCursorRectangle:
    case Qt::ImAnchorRectangle:
        return cursorRect();
    case Qt::ImFont:
        return m_document ? m_document->defaultFont() : QFont();
    case Qt::ImCursorPosition:
        return m_cursor.position();
    case Qt::ImCurrentSelection:
        return m_cursor.selectedText();
    case Qt::ImTextBeforeCursor:
    case Qt::ImSurroundingText:
        if (!m_document)
            return QString();
        return m_document->toPlainText().left(m_cursor.position());
    case Qt::ImTextAfterCursor:
        if (!m_document)
            return QString();
        return m_document->toPlainText().mid(m_cursor.position());
    default:
        break;
    }
    return QWidget::inputMethodQuery(query);
}

void PagedEditorWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    m_cursorVisible = true;
    m_blinkTimer->start();
    update();
}

void PagedEditorWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    m_blinkTimer->stop();
    m_cursorVisible = false;
    m_dragging = false;
    m_autoScrollTimer->stop();
    update();
}

void PagedEditorWidget::contextMenuEvent(QContextMenuEvent *event)
{
    const int boxIdx = hitTestFloatingBox(event->pos());
    if (boxIdx >= 0) {
        m_selectedBoxId = m_floatBoxes.at(boxIdx).id;
        update();
        QMenu menu(this);
        QAction *edit = menu.addAction(tr("编辑文字"));
        QAction *del = menu.addAction(tr("删除文本框"));
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == edit)
            openBoxEditor(m_selectedBoxId);
        else if (chosen == del)
            removeFloatingTextBox(m_selectedBoxId);
        event->accept();
        return;
    }
    if (contextMenuPolicy() == Qt::CustomContextMenu) {
        event->accept();
        QWidget::contextMenuEvent(event); // emits customContextMenuRequested(pos)
        return;
    }
    QMenu *menu = createStandardContextMenu();
    menu->exec(event->globalPos());
    delete menu;
}

void PagedEditorWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    relayoutDocument();
    updateScrollBar();
    ensureCursorVisible();
}

void PagedEditorWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_readOnly || !dragCarriesImage(event)) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void PagedEditorWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_readOnly || !dragCarriesImage(event)) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void PagedEditorWidget::dropEvent(QDropEvent *event)
{
    if (m_readOnly || !event->mimeData()) {
        event->ignore();
        return;
    }
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (isImagePath(path)) {
            emit imageDropped(path);
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void PagedEditorWidget::updateMetrics()
{
    const QSizeF pageMm = m_layout.pageSizeMm();
    const QMarginsF m = m_layout.marginsMm;
    m_pageWidth = qMax(120.0, pageMm.width() * kMmToPx);
    m_pageHeight = qMax(160.0, pageMm.height() * kMmToPx);
    m_contentLeft = m.left() * kMmToPx;
    m_contentTop = (m.top() + m_layout.headerDistanceMm) * kMmToPx;
    m_contentWidth = qMax(40.0, m_pageWidth - (m.left() + m.right()) * kMmToPx);
    m_contentHeight = qMax(60.0,
                           m_pageHeight - (m.top() + m.bottom()) * kMmToPx
                               - (m_layout.headerDistanceMm + m_layout.footerDistanceMm) * kMmToPx);
    m_gap = 28;
    m_topPad = 24;
}

void PagedEditorWidget::relayoutDocument()
{
    if (!m_document)
        return;
    m_document->setDocumentMargin(0);
    if (m_pageMode)
        recomputePagination();
    else
        applyContinuousLayout();
}

void PagedEditorWidget::applyContinuousLayout()
{
    if (!m_document)
        return;
    m_document->setPageSize(QSizeF(m_contentWidth, -1));
    (void)m_document->documentLayout()->documentSize();
    m_pageRanges.clear();
    m_pageRanges.append(PageRange{0.0, qMax(m_contentHeight, m_document->size().height())});
    m_pageCount = 1;
}

void PagedEditorWidget::recomputePagination()
{
    if (!m_document || m_recomputingPagination)
        return;
    m_recomputingPagination = true;

    auto *layout = m_document->documentLayout();
    (void)layout->documentSize(); // force full layout so all line heights are valid

    qreal maxLineHeight = 1.0;
    for (QTextBlock b = m_document->begin(); b.isValid(); b = b.next()) {
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i)
            maxLineHeight = qMax(maxLineHeight, tl->lineAt(i).height());
    }
    m_maxLineHeight = maxLineHeight;

    // Qt places the line that does not fit at exactly the page height, then
    // starts the next page after it. Shrink the layout page height by the max
    // line height so that overflowing line still ends inside the content box.
    m_layoutPageHeight = qMax(120.0, m_contentHeight - maxLineHeight - 1.0);
    m_document->setPageSize(QSizeF(m_contentWidth, m_layoutPageHeight));
    (void)layout->documentSize();

    // Attribute every LINE to a page by its actual doc-y position. Qt clamps
    // the first line that does not fit to exactly the layout page height, so
    // that line lands on the next page here — the same behavior as Word
    // pushing a line that does not fit to the next page.
    QVector<qreal> minTop;
    QVector<qreal> maxBottom;
    QVector<int> startBlocks;
    for (QTextBlock b = m_document->begin(); b.isValid(); b = b.next()) {
        const QRectF br = layout->blockBoundingRect(b);
        if (!br.isValid())
            continue;
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i) {
            const QTextLine ln = tl->lineAt(i);
            const qreal top = br.top() + ln.y();
            const int page = qMax(0, int(std::floor(top / m_layoutPageHeight)));
            while (minTop.size() <= page) {
                minTop.append(std::numeric_limits<qreal>::max());
                maxBottom.append(std::numeric_limits<qreal>::lowest());
            }
            minTop[page] = qMin(minTop[page], top);
            maxBottom[page] = qMax(maxBottom[page], top + ln.height());
            if (startBlocks.size() <= page)
                startBlocks.resize(page + 1, b.blockNumber());
        }
    }

    m_pageRanges.clear();
    if (minTop.isEmpty()) {
        m_pageRanges.append(PageRange{0.0, m_contentHeight});
    } else {
        m_pageRanges.reserve(minTop.size());
        for (int i = 0; i < minTop.size(); ++i)
            m_pageRanges.append(PageRange{minTop.at(i),
                                          qMax(minTop.at(i) + 1.0, maxBottom.at(i))});
    }
    m_pageCount = m_pageRanges.size();
    m_pageStartBlocks = startBlocks;
    m_rangeBuiltToPage = m_pageCount - 1;
    m_rebuildBlock = -1;
    m_maxLineHeightDirty = false;
    m_lastChangePos = -1;
    m_recomputingPagination = false;
}

void PagedEditorWidget::markDirty(int position)
{
    if (!m_document)
        return;
    const QTextBlock b = m_document->findBlock(position);
    const int bn = b.isValid() ? b.blockNumber() : 0;
    m_rebuildBlock = (m_rebuildBlock < 0) ? bn : qMin(m_rebuildBlock, bn);

    // Pages before the block's page are untouched; everything from that page on
    // must be re-walked. pageStartBlocks is still valid up to the old built page.
    int page = 0;
    for (int i = 0; i < m_pageStartBlocks.size(); ++i) {
        const int sb = m_pageStartBlocks.at(i);
        if (sb >= 0 && sb <= bn)
            page = i;
        else
            break;
    }
    // The edit page's end is now unknown — reset it so the rebuild does not
    // keep a stale (possibly too large) end after deletions.
    if (page < m_pageRanges.size())
        m_pageRanges[page].end = m_pageRanges[page].start;
    m_rangeBuiltToPage = qMin(m_rangeBuiltToPage, page - 1);
}

void PagedEditorWidget::ensurePageRangeSize(int pageIndex) const
{
    if (m_pageRanges.size() <= pageIndex)
        m_pageRanges.resize(pageIndex + 1, PageRange{0.0, 0.0});
}

void PagedEditorWidget::ensureRangesThroughPage(int pageIndex) const
{
    if (!m_document || !m_pageMode)
        return;
    if (m_maxLineHeightDirty) {
        if (!m_recomputingPagination)
            const_cast<PagedEditorWidget *>(this)->recomputePagination();
        return;
    }
    if (m_rebuildBlock < 0 || pageIndex <= m_rangeBuiltToPage)
        return;

    auto *layout = m_document->documentLayout();
    const int startPage = qMax(0, m_rangeBuiltToPage + 1);
    int blockNum = m_rebuildBlock;
    int page = startPage;
    qreal minTop = std::numeric_limits<qreal>::max();
    qreal maxBottom = std::numeric_limits<qreal>::lowest();
    const bool editInFirstBlock =
        blockNum <= (page < m_pageStartBlocks.size() ? m_pageStartBlocks.at(page) : -1);
    if (!editInFirstBlock && page < m_pageRanges.size())
        minTop = m_pageRanges.at(page).start;

    m_recomputingPagination = true;
    auto finishCurrentPage = [&]() -> bool {
        if (minTop >= std::numeric_limits<qreal>::max()) {
            // The page turned out empty / start moved — safe full pass.
            m_recomputingPagination = false;
            const_cast<PagedEditorWidget *>(this)->recomputePagination();
            return false;
        }
        ensurePageRangeSize(page);
        m_pageRanges[page].end = qMax(m_pageRanges[page].end, maxBottom);
        if (page == startPage) {
            if (editInFirstBlock)
                m_pageRanges[page].start = minTop;
            else
                m_pageRanges[page].start = qMin(m_pageRanges[page].start, minTop);
        }
        if (m_pageStartBlocks.size() <= page)
            m_pageStartBlocks.resize(page + 1, -1);
        if (page > startPage || m_pageStartBlocks.at(page) < 0)
            m_pageStartBlocks[page] = blockNum;
        return true;
    };

    while (blockNum < m_document->blockCount()) {
        const QTextBlock b = m_document->findBlockByNumber(blockNum);
        const QRectF br = layout->blockBoundingRect(b);
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i) {
            const QTextLine ln = tl->lineAt(i);
            const qreal top = br.top() + ln.y();
            const int lp = qMax(0, int(std::floor(top / m_layoutPageHeight)));
            const qreal bottom = top + ln.height();
            if (lp < page) {
                if (lp != page - 1) {
                    // Content moved onto a much earlier page (large delete) —
                    // safe path: full pass.
                    m_recomputingPagination = false;
                    const_cast<PagedEditorWidget *>(this)->recomputePagination();
                    return;
                }
                // Boundary off-by-one: the first walked line sits on the page
                // before the expected start (a block straddles the break).
                // Back up one page and rebuild its end from the walk.
                page = lp;
                maxBottom = std::numeric_limits<qreal>::lowest();
                if (page < m_pageRanges.size())
                    m_pageRanges[page].end = m_pageRanges[page].start;
            }
            if (lp > page) {
                if (!finishCurrentPage())
                    return;
                if (lp > page + 1) {
                    // Empty pages in between — drop them.
                    m_pageRanges.resize(page + 1);
                    m_pageStartBlocks.resize(qMax(0, page));
                }
                page = lp;
                minTop = top;
                maxBottom = bottom;
                ensurePageRangeSize(page);
                m_pageRanges[page].start = top;
                if (page > pageIndex) {
                    if (m_pageStartBlocks.size() <= page)
                        m_pageStartBlocks.resize(page + 1, -1);
                    m_pageStartBlocks[page] = blockNum;
                    m_pageCount = qMax(m_pageCount, page + 1);
                    m_rangeBuiltToPage = pageIndex;
                    m_rebuildBlock = blockNum; // continue from this block next time
                    m_recomputingPagination = false;
                    const_cast<PagedEditorWidget *>(this)->updateScrollBar();
                    return;
                }
            } else {
                minTop = qMin(minTop, top);
                maxBottom = qMax(maxBottom, bottom);
            }
        }
        ++blockNum;
    }

    // Reached the end of the document: everything is accurate now.
    if (minTop >= std::numeric_limits<qreal>::max()) {
        m_recomputingPagination = false;
        const_cast<PagedEditorWidget *>(this)->recomputePagination();
        return;
    }
    if (!finishCurrentPage())
        return;
    m_pageRanges.resize(page + 1);
    m_pageStartBlocks.resize(page + 1);
    m_pageCount = page + 1;
    m_rangeBuiltToPage = page;
    m_rebuildBlock = -1;
    m_recomputingPagination = false;
    const_cast<PagedEditorWidget *>(this)->updateScrollBar();
}

int PagedEditorWidget::pageIndexForDocY(qreal docY) const
{
    if (m_pageRanges.isEmpty())
        return 0;
    int idx = 0;
    for (int i = 0; i < m_pageRanges.size(); ++i) {
        if (docY >= m_pageRanges.at(i).start)
            idx = i;
        else
            break;
    }
    return qBound(0, idx, m_pageRanges.size() - 1);
}

void PagedEditorWidget::updatePageCount()
{
    m_pageCount = qMax(1, m_pageRanges.size());
}

void PagedEditorWidget::updateScrollBar()
{
    const qreal f = zoomScale();
    qreal total = 0.0;
    if (m_pageMode) {
        total = (2 * m_topPad + m_pageCount * m_pageHeight
                 + qMax(0, m_pageCount - 1) * m_gap)
                * f;
    } else {
        const qreal contentH = m_pageRanges.isEmpty() ? m_contentHeight
                                                      : m_pageRanges.first().end;
        total = (2 * m_topPad + qMax(1.0, contentH)) * f;
    }
    m_vScroll->setRange(0, qMax(0, int(total) - height()));
    m_vScroll->setPageStep(qMax(1, height()));
    m_vScroll->setSingleStep(qMax(1, qRound(24 * f)));
}

void PagedEditorWidget::ensureCursorVisible()
{
    if (!m_document || !isVisible())
        return;
    const QRectF r = cursorRect();
    if (!r.isValid())
        return;
    const int scrollY = m_vScroll->value();
    if (r.top() - scrollY < 8)
        scrollBy(int(r.top() - scrollY) - 8);
    else if (r.bottom() - scrollY > height() - kScrollbarWidth - 8)
        scrollBy(int(r.bottom() - scrollY) - (height() - kScrollbarWidth - 8));
}

QRectF PagedEditorWidget::pageRectInWidget(int pageIndex) const
{
    const qreal f = zoomScale();
    if (!m_pageMode) {
        const qreal w = m_pageWidth * f;
        const qreal x = qMax(0.0, (width() - w) / 2.0);
        const qreal h = qMax(1.0, (m_pageRanges.isEmpty() ? m_contentHeight
                                                          : m_pageRanges.first().end))
                        * f;
        return QRectF(x, -m_vScroll->value(), w, h);
    }
    const qreal x = qMax(0.0, (width() - m_pageWidth * f) / 2.0);
    const qreal y = (m_topPad + pageIndex * (m_pageHeight + m_gap)) * f
                    - m_vScroll->value();
    return QRectF(x, y, m_pageWidth * f, m_pageHeight * f);
}

QRectF PagedEditorWidget::contentRectInWidget(int pageIndex) const
{
    const QRectF pr = pageRectInWidget(pageIndex);
    const qreal f = zoomScale();
    if (!m_pageMode) {
        const qreal h = qMax(1.0, (m_pageRanges.isEmpty() ? m_contentHeight
                                                          : m_pageRanges.first().end))
                        * f;
        return QRectF(pr.left() + m_contentLeft, pr.top() + m_contentTop,
                      m_contentWidth * f, h);
    }
    return QRectF(pr.left() + m_contentLeft, pr.top() + m_contentTop,
                  m_contentWidth * f, m_contentHeight * f);
}

int PagedEditorWidget::pageIndexAt(const QPoint &widgetPos) const
{
    if (!m_pageMode || m_pageCount <= 0 || m_pageHeight <= 0)
        return 0;
    const qreal f = zoomScale();
    const qreal y = widgetPos.y() + m_vScroll->value() - m_topPad * f;
    const qreal stride = (m_pageHeight + m_gap) * f;
    if (y <= 0)
        return 0;
    return qMin(m_pageCount - 1, int(std::floor(y / stride)));
}

QPointF PagedEditorWidget::documentPointAt(const QPoint &widgetPos) const
{
    const qreal f = zoomScale();
    const int k = pageIndexAt(widgetPos);
    ensureRangesThroughPage(k);
    const QRectF cr = contentRectInWidget(k);
    const qreal x = qBound(0.0, (widgetPos.x() - cr.left()) / f, m_contentWidth);
    const PageRange range = m_pageRanges.at(k);
    const qreal y = range.start
                    + qBound(0.0, (widgetPos.y() - cr.top()) / f,
                             range.end - range.start);
    return QPointF(x, y);
}

void PagedEditorWidget::setCursorFromWidget(const QPoint &widgetPos, bool keepAnchor)
{
    if (!m_document)
        return;
    const QPointF docPoint = documentPointAt(widgetPos);
    int hit = m_document->documentLayout()->hitTest(docPoint, Qt::FuzzyHit);
    if (hit < 0) {
        // Empty area (e.g. a blank page): snap to the nearest block.
        qreal bestDist = std::numeric_limits<qreal>::max();
        hit = m_document->characterCount() - 1;
        for (QTextBlock b = m_document->begin(); b.isValid(); b = b.next()) {
            const QRectF r = m_document->documentLayout()->blockBoundingRect(b);
            const qreal d = qAbs(r.center().y() - docPoint.y())
                            + qAbs(r.center().x() - docPoint.x());
            if (d < bestDist) {
                bestDist = d;
                hit = b.position();
            }
        }
    }
    m_cursor.setPosition(hit, keepAnchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
    afterCursorMove();
}

void PagedEditorWidget::drawPage(QPainter *painter, int pageIndex)
{
    ensureRangesThroughPage(pageIndex);
    const QRectF pr = pageRectInWidget(pageIndex);
    if (!pr.intersects(rect()))
        return;

    const qreal f = zoomScale();
    if (m_pageMode) {
        // Soft drop shadow approximating the previous QGraphicsDropShadowEffect.
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        for (int i = 6; i >= 1; --i) {
            painter->setBrush(QColor(30, 40, 55, 4 + (6 - i) * 3));
            painter->drawRoundedRect(pr.translated(0, 4 + i), 2, 2);
        }
        painter->restore();
    }

    // Page body.
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(AppStyle::border()), 1));
    painter->setBrush(Qt::white);
    painter->drawRoundedRect(pr, 2, 2);
    painter->restore();

    if (m_pageMode)
        drawHeaderFooter(painter, pageIndex, pr);

    if (!m_document)
        return;

    // Document content of this page (document layout positions pages at y = k*contentHeight).
    const QRectF cr = contentRectInWidget(pageIndex);
    painter->save();
    painter->setClipRect(cr);
    painter->translate(cr.topLeft());
    painter->scale(f, f);
    // The layout stacks pages continuously with page k starting at
    // m_pageRanges[k].start; shift so this page's content lands in its box.
    painter->translate(0, -m_pageRanges.at(pageIndex).start);

    QAbstractTextDocumentLayout::PaintContext ctx;
    const PageRange range = m_pageRanges.at(pageIndex);
    ctx.clip = QRectF(0, range.start, m_contentWidth, range.end - range.start);
    ctx.palette = palette();
    if (m_cursor.hasSelection()) {
        QAbstractTextDocumentLayout::Selection sel;
        sel.cursor = m_cursor;
        sel.format.setBackground(palette().color(QPalette::Highlight));
        sel.format.setForeground(palette().color(QPalette::HighlightedText));
        ctx.selections.append(sel);
    }
    m_document->documentLayout()->draw(painter, ctx);
    painter->restore();

    // Blinking cursor (drawn by us so the blink phase is under our control).
    if (hasFocus() && m_cursorVisible && !m_cursor.hasSelection()) {
        const QRectF caret = cursorRect();
        if (caret.intersects(cr)) {
            painter->save();
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(AppStyle::text()));
            painter->drawRect(caret);
            painter->restore();
        }
    }

    if (m_pageMode)
        paintGridLines(painter, cr);
    paintFloatingBoxes(painter, pageIndex);
}

void PagedEditorWidget::drawHeaderFooter(QPainter *painter, int pageIndex,
                                         const QRectF &pageRect)
{
    const qreal f = zoomScale();
    painter->save();
    painter->translate(pageRect.topLeft());
    painter->scale(f, f);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    QFont hf = font();
    hf.setPointSizeF(qMax(8.0, hf.pointSizeF() - 1.0));
    painter->setFont(hf);
    painter->setPen(QColor(AppStyle::textMuted()));
    const QMarginsF m = m_layout.marginsMm;

    const QString header = m_headerFooter.headerForPage(pageIndex);
    if (!header.isEmpty()) {
        const qreal y = (m.top() + m_layout.headerDistanceMm) / 2.0 * kMmToPx
                        - painter->fontMetrics().height() / 2.0;
        painter->drawText(QRectF(0, y, m_pageWidth, painter->fontMetrics().height()),
                          Qt::AlignHCenter | Qt::AlignVCenter, header);
    }

    const QString footer = m_headerFooter.composedFooter(pageIndex, m_pageCount);
    if (!footer.isEmpty()) {
        const qreal y = m_pageHeight
                        - (m.bottom() + m_layout.footerDistanceMm) / 2.0 * kMmToPx
                        - painter->fontMetrics().height() / 2.0;
        painter->drawText(QRectF(0, y, m_pageWidth, painter->fontMetrics().height()),
                          Qt::AlignHCenter | Qt::AlignVCenter, footer);
    }
    painter->restore();
}

void PagedEditorWidget::moveCursor(QTextCursor::MoveOperation op,
                                   QTextCursor::MoveMode mode, int n)
{
    m_cursor.movePosition(op, mode, n);
    afterCursorMove();
}

void PagedEditorWidget::insertText(const QString &text)
{
    if (m_readOnly || text.isEmpty())
        return;
    if (m_cursor.hasSelection())
        m_cursor.removeSelectedText();
    if (m_currentCharFormat.isEmpty())
        m_cursor.insertText(text);
    else
        m_cursor.insertText(text, m_currentCharFormat);
    afterCursorMove();
}

void PagedEditorWidget::deleteChar(bool backspace)
{
    if (m_readOnly)
        return;
    if (m_cursor.hasSelection()) {
        m_cursor.removeSelectedText();
        afterCursorMove();
        return;
    }
    QTextCursor c = m_cursor;
    if (backspace)
        c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    else
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    if (c.hasSelection()) {
        c.removeSelectedText();
        m_cursor = c;
        afterCursorMove();
    }
}

void PagedEditorWidget::removePreedit()
{
    if (m_preeditLength <= 0)
        return;
    QTextCursor c = m_cursor;
    c.setPosition(c.position() - m_preeditLength, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    m_preeditLength = 0;
}

void PagedEditorWidget::insertPreedit(const QString &text)
{
    if (text.isEmpty())
        return;
    // The preedit is underlined; remember the format at the cursor BEFORE the
    // composition so committed text does not inherit the underline.
    if (m_preeditLength == 0)
        m_preeditBaseFormat = m_cursor.charFormat();
    QTextCursor c = m_cursor;
    c.insertText(text, preeditFormat(palette()));
    m_preeditLength = text.length();
    m_cursor = c;
}

void PagedEditorWidget::scrollBy(int deltaY)
{
    if (m_boxEditor && m_boxEditor->isVisible())
        commitBoxEditor();
    m_vScroll->setValue(m_vScroll->value() + deltaY);
}

void PagedEditorWidget::resetCursorBlink()
{
    m_cursorVisible = true;
    if (hasFocus())
        m_blinkTimer->start();
}

void PagedEditorWidget::afterCursorMove()
{
    updatePageCount();
    resetCursorBlink();
    ensureCursorVisible();
    update();
    emit cursorPositionChanged();
    emit pageInfoChanged();
    const bool hasSelection = m_cursor.hasSelection();
    if (hasSelection != m_lastHadSelection) {
        m_lastHadSelection = hasSelection;
        emit copyAvailable(hasSelection);
        emit selectionChanged();
    }
    // While composing, the format at the cursor is the underlined preedit —
    // keep the pre-composition format so commits stay clean.
    if (m_preeditLength == 0)
        m_currentCharFormat = m_cursor.charFormat();
    emit currentCharFormatChanged(currentCharFormat());
}

void PagedEditorWidget::afterDocumentChange()
{
    if (!m_pageMode) {
        applyContinuousLayout();
        updatePageCount();
        updateScrollBar();
        update();
        return;
    }
    if (!m_burstHasChars) {
        // Formatting-only cycle: no text geometry changed. The 40ms format
        // check decides whether a real format change needs a full pass.
        if (m_burstHasFormatOnly && m_formatCheckTimer)
            m_formatCheckTimer->start();
        update();
        return;
    }
    m_burstHasChars = false;
    m_burstHasFormatOnly = false;
    markDirty(m_lastChangePos);
    m_lastChangePos = -1;
    if (m_fullPassTimer)
        m_fullPassTimer->start(); // debounced full pass for exact page count/scrollbar
    (void)currentPageIndex();     // rebuilds ranges through the caret page immediately
    updateScrollBar();
    updateEditRegion();
}

void PagedEditorWidget::updateEditRegion()
{
    if (!m_document || m_pageCount <= 0) {
        update();
        return;
    }
    const int page = currentPageIndex();
    const QRectF content = contentRectInWidget(page);
    const QRectF caret = cursorRect();
    const qreal y0 = qMax(content.top(), caret.top() - 24);
    QRect region(qRound(content.left()), qRound(y0), qRound(content.width()),
                 qRound(content.bottom() - y0 + 8));
    if (page + 1 < m_pageCount) {
        const QRectF next = contentRectInWidget(page + 1);
        region |= QRect(qRound(next.left()), qRound(next.top()), qRound(next.width()), 48);
    }
    update(region);
}

void PagedEditorWidget::setPageLayout(const PageLayoutSettings &layout)
{
    m_layout = layout;
    updateMetrics();
    relayoutDocument();
    updateScrollBar();
    update();
    emit pageInfoChanged();
    emit pageGeometryChanged();
}

#include "pagededitorwidget.moc"

void PagedEditorWidget::setHeaderFooter(const HeaderFooterSettings &settings)
{
    m_headerFooter = settings;
    update();
}

void PagedEditorWidget::setZoomPercent(int percent)
{
    percent = qBound(50, percent, 200);
    if (percent == m_zoomPercent)
        return;
    m_zoomPercent = percent;
    updateScrollBar();
    update();
    emit pageGeometryChanged();
}

void PagedEditorWidget::setPageMode(bool pageMode)
{
    if (m_pageMode == pageMode)
        return;
    m_pageMode = pageMode;
    relayoutDocument();
    updateScrollBar();
    update();
    emit pageInfoChanged();
    emit pageGeometryChanged();
}
