#include "qtextadapter.h"
#include "floatingtextbox.h"
#include "imageprops.h"
#include "reviewnotes.h"
#include "styleutils.h"
#include "tablegeometry.h"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextLength>
#include <QTextTable>
#include <QTextTableCell>
#include <QUrl>
#include <QVariant>
#include <QPixmap>

#include <cmath>
#include <limits>
#include <utility>

#include <QHash>
#include <QSet>

namespace Engine {
namespace QTextAdapter {
namespace {

constexpr qreal kPxToPt = 0.75; // 96 dpi → points
constexpr auto kFootnoteAppendixTitle = QStringView(u"脚注");
constexpr auto kEndnoteAppendixTitle = QStringView(u"尾注");

QString romanMarker(int n)
{
    static const char *kRoman[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x",
                                   "xi", "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii",
                                   "xix", "xx"};
    if (n >= 1 && n <= 20)
        return QString::fromLatin1(kRoman[n - 1]);
    return QString::number(n);
}

struct AdaptContext {
    DocumentModel *model = nullptr;
    QHash<QString, int> footnoteNumbers;
    QHash<QString, int> endnoteNumbers;
    QSet<QString> seenComments;
    bool stop = false;
};

CharStyle styleFromFormat(const QTextCharFormat &fmt, const QFont &defaultFont)
{
    CharStyle style;
    style.font = fmt.font();
    if (style.font.family().isEmpty())
        style.font = defaultFont;
    if (style.font.pointSizeF() <= 0)
        style.font.setPointSizeF(defaultFont.pointSizeF() > 0 ? defaultFont.pointSizeF() : 12.0);
    style.bold = fmt.fontWeight() >= QFont::Bold;
    style.italic = fmt.fontItalic();
    style.underline = fmt.fontUnderline();
    if (style.bold)
        style.font.setBold(true);
    if (style.italic)
        style.font.setItalic(true);
    if (style.underline)
        style.font.setUnderline(true);

    if (fmt.hasProperty(QTextFormat::ForegroundBrush)
        && fmt.foreground().style() != Qt::NoBrush)
        style.foreground = fmt.foreground().color();
    else
        style.foreground = QColor(0, 0, 0);
    if (fmt.hasProperty(QTextFormat::BackgroundBrush)
        && fmt.background().style() != Qt::NoBrush)
        style.background = fmt.background().color();
    style.superscript = fmt.verticalAlignment() == QTextCharFormat::AlignSuperScript;
    return style;
}

QImage loadImage(const QTextDocument *document, const QTextImageFormat &img)
{
    if (!document)
        return {};
    const QString name = img.name();
    if (name.isEmpty())
        return {};

    const QUrl url(name);
    const QVariant res = document->resource(QTextDocument::ImageResource, url);
    if (res.canConvert<QImage>())
        return res.value<QImage>();
    if (res.canConvert<QPixmap>())
        return res.value<QPixmap>().toImage();

    if (url.isLocalFile()) {
        QImage fileImg(url.toLocalFile());
        if (!fileImg.isNull())
            return fileImg;
    }
    return {};
}

DocRun imageRunFromFormat(const QTextDocument *document, const QTextImageFormat &img)
{
    DocRun run;
    run.isAtomic = true;
    run.image = loadImage(document, img);
    run.text = QStringLiteral("[image]");

    qreal w = img.width();
    qreal h = img.height();
    if (w <= 0 && !run.image.isNull())
        w = run.image.width();
    if (h <= 0 && !run.image.isNull())
        h = run.image.height();
    if (w <= 0)
        w = 320;
    if (h <= 0)
        h = 240;

    run.atomicWidthPt = w * kPxToPt;
    run.atomicHeightPt = h * kPxToPt;
    run.imageWrap = int(ImageProps::wrapOf(img));
    run.imageAlign = int(ImageProps::alignOf(img));
    return run;
}

DocParagraph paragraphFromBlock(const QTextBlock &block,
                                const QTextDocument *document,
                                const QFont &defaultFont,
                                AdaptContext *ctx)
{
    DocParagraph para;
    para.documentPosition = block.position();
    para.headingLevel = block.blockFormat().headingLevel();
    para.styleId = StyleUtils::detectStyle(QTextCursor(block));
    para.pageBreakBefore =
        block.blockFormat().pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore;

    if (para.headingLevel == 1)
        para.spaceAfterPt = 12.0;
    else if (para.headingLevel >= 2)
        para.spaceAfterPt = 8.0;

    for (auto it = block.begin(); !(it.atEnd()); ++it) {
        const QTextFragment frag = it.fragment();
        if (!frag.isValid())
            continue;
        const QTextCharFormat fmt = frag.charFormat();
        DocRun run;
        if (fmt.isImageFormat()) {
            run = imageRunFromFormat(document, fmt.toImageFormat());
        } else {
            run.text = frag.text();
            run.text.remove(QChar::ObjectReplacementCharacter);
            run.style = styleFromFormat(fmt, defaultFont);
            if (fmt.hasProperty(ReviewNotes::FootnoteIdProperty)) {
                const QString id = fmt.property(ReviewNotes::FootnoteIdProperty).toString();
                run.footnoteId = id;
                if (ctx && !id.isEmpty()) {
                    if (!ctx->footnoteNumbers.contains(id)) {
                        const int n = ctx->footnoteNumbers.size() + 1;
                        ctx->footnoteNumbers.insert(id, n);
                        if (ctx->model) {
                            const QVariantMap map =
                                document
                                    ->resource(QTextDocument::UserResource,
                                               QUrl(QStringLiteral("newword://footnotes")))
                                    .toMap();
                            ctx->model->footnoteBodies.insert(id, map.value(id).toString());
                            ctx->model->footnoteOrder.append(id);
                        }
                    }
                    run.footnoteNumber = ctx->footnoteNumbers.value(id);
                    run.text = QString::number(run.footnoteNumber);
                    run.style.superscript = true;
                }
            }
            if (fmt.hasProperty(ReviewNotes::EndnoteIdProperty)) {
                const QString id = fmt.property(ReviewNotes::EndnoteIdProperty).toString();
                run.endnoteId = id;
                if (ctx && !id.isEmpty()) {
                    if (!ctx->endnoteNumbers.contains(id)) {
                        const int n = ctx->endnoteNumbers.size() + 1;
                        ctx->endnoteNumbers.insert(id, n);
                        if (ctx->model) {
                            const QVariantMap map =
                                document
                                    ->resource(QTextDocument::UserResource,
                                               QUrl(QStringLiteral("newword://endnotes")))
                                    .toMap();
                            ctx->model->endnoteBodies.insert(id, map.value(id).toString());
                            ctx->model->endnoteOrder.append(id);
                        }
                    }
                    run.endnoteNumber = ctx->endnoteNumbers.value(id);
                    run.text = romanMarker(run.endnoteNumber);
                    run.style.superscript = true;
                }
            }
            if (fmt.hasProperty(ReviewNotes::CommentIdProperty)) {
                const QString id = fmt.property(ReviewNotes::CommentIdProperty).toString();
                run.commentId = id;
                if (!run.style.background.isValid())
                    run.style.background = QColor(255, 249, 196);
                if (ctx && ctx->model && !id.isEmpty() && !ctx->seenComments.contains(id)) {
                    ctx->seenComments.insert(id);
                    const QVariantMap map =
                        document
                            ->resource(QTextDocument::UserResource,
                                       QUrl(QStringLiteral("newword://comments")))
                            .toMap();
                    const QVariantMap entry = map.value(id).toMap();
                    DocComment comment;
                    comment.author = entry.value(QStringLiteral("author")).toString();
                    comment.text = entry.value(QStringLiteral("text")).toString();
                    ctx->model->comments.insert(id, comment);
                    ctx->model->commentOrder.append(id);
                }
            }
        }
        if (!run.text.isEmpty() || run.isAtomic)
            para.runs.append(run);
    }

    if (para.runs.isEmpty()) {
        DocRun empty;
        empty.style.font = defaultFont;
        if (empty.style.font.pointSizeF() <= 0)
            empty.style.font.setPointSizeF(12.0);
        para.runs.append(empty);
    }
    return para;
}

DocTableCell cellFromTableCell(const QTextTableCell &cell,
                               const QTextDocument *document,
                               const QFont &defaultFont,
                               AdaptContext *ctx)
{
    DocTableCell out;
    const QTextCharFormat cellFmt = cell.format();
    if (cellFmt.hasProperty(QTextFormat::BackgroundBrush)
        && cellFmt.background().style() != Qt::NoBrush)
        out.background = cellFmt.background().color();

    QTextCursor end = cell.lastCursorPosition();
    for (QTextBlock block = cell.firstCursorPosition().block();
         block.isValid() && block.position() <= end.block().position();
         block = block.next()) {
        out.paragraphs.append(paragraphFromBlock(block, document, defaultFont, ctx));
        if (block == end.block())
            break;
    }
    if (out.paragraphs.isEmpty()) {
        DocParagraph blank;
        DocRun empty;
        empty.style.font = defaultFont;
        if (empty.style.font.pointSizeF() <= 0)
            empty.style.font.setPointSizeF(12.0);
        blank.runs.append(empty);
        blank.spaceAfterPt = 0;
        out.paragraphs.append(blank);
    }
    return out;
}

DocTable tableFromQTextTable(QTextTable *table,
                             const QTextDocument *document,
                             const QFont &defaultFont,
                             AdaptContext *ctx)
{
    DocTable out;
    if (!table)
        return out;

    out.documentPosition = table->firstPosition();
    out.columnCount = table->columns();
    const QTextTableFormat tf = table->format();
    out.borderPt = qMax(0.0, tf.border());
    if (out.borderPt <= 0 && tf.borderStyle() != QTextFrameFormat::BorderStyle_None)
        out.borderPt = 0.5;
    out.cellPaddingPt = qMax(2.0, tf.cellPadding());

    const QVector<QTextLength> constraints = tf.columnWidthConstraints();
    out.columnWeights.resize(out.columnCount);
    qreal weightSum = 0;
    for (int c = 0; c < out.columnCount; ++c) {
        qreal w = 1.0;
        if (c < constraints.size()) {
            const QTextLength &len = constraints.at(c);
            if (len.type() == QTextLength::PercentageLength)
                w = qMax(0.01, len.rawValue());
            else if (len.type() == QTextLength::FixedLength)
                w = qMax(0.01, len.rawValue());
        }
        out.columnWeights[c] = w;
        weightSum += w;
    }
    if (weightSum <= 0) {
        for (int c = 0; c < out.columnCount; ++c)
            out.columnWeights[c] = 1.0;
    }

    out.rows.resize(table->rows());
    out.rowMinHeightsPt.resize(table->rows());
    for (int r = 0; r < table->rows(); ++r) {
        out.rows[r].resize(out.columnCount);
        qreal rowMin = 0;
        for (int c = 0; c < out.columnCount; ++c) {
            const QTextTableCell qtCell = table->cellAt(r, c);
            if (!qtCell.isValid())
                continue;
            if (qtCell.row() != r || qtCell.column() != c) {
                DocTableCell covered;
                covered.covered = true;
                covered.columnSpan = 1;
                covered.rowSpan = 1;
                out.rows[r][c] = covered;
                continue;
            }
            DocTableCell cell = cellFromTableCell(qtCell, document, defaultFont, ctx);
            cell.columnSpan = qMax(1, qtCell.columnSpan());
            cell.rowSpan = qMax(1, qtCell.rowSpan());
            cell.covered = false;
            out.rows[r][c] = cell;
            const QTextCharFormat cellFmt = qtCell.format();
            if (cellFmt.hasProperty(TableGeometry::RowMinHeightProperty))
                rowMin = qMax(rowMin, cellFmt.property(TableGeometry::RowMinHeightProperty).toReal());
        }
        out.rowMinHeightsPt[r] = rowMin;
    }
    return out;
}

void appendFromFrame(QTextFrame *frame,
                     DocSection *section,
                     const QTextDocument *document,
                     const QFont &defaultFont,
                     int fromPos,
                     AdaptContext *ctx)
{
    if (!frame || !section || (ctx && ctx->stop))
        return;

    for (QTextFrame::iterator it = frame->begin(); !(it.atEnd()); ++it) {
        if (ctx && ctx->stop)
            break;
        if (QTextFrame *child = it.currentFrame()) {
            if (auto *table = qobject_cast<QTextTable *>(child)) {
                if (table->lastPosition() < fromPos)
                    continue;
                DocBlock block;
                block.kind = DocBlock::Kind::Table;
                block.table = tableFromQTextTable(table, document, defaultFont, ctx);
                block.documentPosition = block.table.documentPosition;
                section->blocks.append(block);
            } else {
                appendFromFrame(child, section, document, defaultFont, fromPos, ctx);
            }
        } else if (it.currentBlock().isValid()) {
            const QTextBlock tb = it.currentBlock();
            if (tb.position() + tb.length() <= fromPos)
                continue;
            // Live editor keeps trailing「脚注」「尾注」appendices — skip for engine layout.
            if (tb.blockFormat().headingLevel() == 2) {
                const QString title = tb.text().trimmed();
                if (title == kFootnoteAppendixTitle || title == kEndnoteAppendixTitle) {
                    if (ctx)
                        ctx->stop = true;
                    break;
                }
            }
            DocBlock block;
            block.kind = DocBlock::Kind::Paragraph;
            block.paragraph = paragraphFromBlock(tb, document, defaultFont, ctx);
            block.documentPosition = block.paragraph.documentPosition;
            section->blocks.append(block);
        }
    }
}

DocumentModel buildModel(const QTextDocument *document,
                         const PageLayoutSettings &pageSetup,
                         int fromPos,
                         QVector<DocBlock> prefix)
{
    DocumentModel model;
    if (!document)
        return model;

    AdaptContext ctx;
    ctx.model = &model;

    DocSection section;
    section.pageSetup = pageSetup;
    section.blocks = std::move(prefix);
    appendFromFrame(document->rootFrame(), &section, document, document->defaultFont(), fromPos,
                    &ctx);

    if (section.blocks.isEmpty()) {
        DocBlock block;
        block.kind = DocBlock::Kind::Paragraph;
        DocRun empty;
        empty.style.font = document->defaultFont();
        if (empty.style.font.pointSizeF() <= 0)
            empty.style.font.setPointSizeF(12.0);
        block.paragraph.runs.append(empty);
        section.blocks.append(block);
    }

    model.sections.append(std::move(section));
    model.floatingBoxes = FloatingTextBoxes::load(document);
    return model;
}

bool setupsEqual(const PageLayoutSettings &a, const PageLayoutSettings &b)
{
    return a.paper == b.paper
        && a.orientation == b.orientation
        && std::abs(a.customWidthMm - b.customWidthMm) < 0.05
        && std::abs(a.customHeightMm - b.customHeightMm) < 0.05
        && std::abs(a.marginsMm.left() - b.marginsMm.left()) < 0.05
        && std::abs(a.marginsMm.top() - b.marginsMm.top()) < 0.05
        && std::abs(a.marginsMm.right() - b.marginsMm.right()) < 0.05
        && std::abs(a.marginsMm.bottom() - b.marginsMm.bottom()) < 0.05
        && std::abs(a.headerDistanceMm - b.headerDistanceMm) < 0.05
        && std::abs(a.footerDistanceMm - b.footerDistanceMm) < 0.05
        && a.columnCount == b.columnCount;
}

} // namespace

DocumentModel fromDocument(const QTextDocument *document, const PageLayoutSettings &pageSetup)
{
    return buildModel(document, pageSetup, 0, {});
}

namespace {

QTextCharFormat charFormatFromStyle(const CharStyle &style)
{
    QTextCharFormat fmt;
    if (!style.font.family().isEmpty())
        fmt.setFontFamilies({style.font.family()});
    if (style.font.pointSizeF() > 0)
        fmt.setFontPointSize(style.font.pointSizeF());
    if (style.bold || style.font.bold())
        fmt.setFontWeight(QFont::Bold);
    if (style.italic || style.font.italic())
        fmt.setFontItalic(true);
    if (style.underline || style.font.underline())
        fmt.setFontUnderline(true);
    if (style.foreground.isValid())
        fmt.setForeground(style.foreground);
    if (style.background.isValid())
        fmt.setBackground(style.background);
    if (style.superscript)
        fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    return fmt;
}

void insertParagraph(QTextCursor *cursor, const DocParagraph &para, int *imageCounter)
{
    if (!cursor)
        return;
    StyleUtils::applyStyle(*cursor, para.styleId);
    if (para.pageBreakBefore) {
        QTextBlockFormat bf = cursor->blockFormat();
        bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        cursor->setBlockFormat(bf);
    }

    bool wrote = false;
    for (const DocRun &run : para.runs) {
        if (run.isAtomic && !run.image.isNull()) {
            const QString name = QStringLiteral("model-image-%1").arg(++(*imageCounter));
            cursor->document()->addResource(QTextDocument::ImageResource, QUrl(name), run.image);
            QTextImageFormat img;
            img.setName(name);
            // pt → px at 96dpi
            if (run.atomicWidthPt > 0)
                img.setWidth(run.atomicWidthPt / 0.75);
            if (run.atomicHeightPt > 0)
                img.setHeight(run.atomicHeightPt / 0.75);
            ImageProps::setWrap(&img, static_cast<ImageProps::Wrap>(run.imageWrap));
            ImageProps::setAlign(&img, Qt::Alignment(run.imageAlign));
            cursor->insertImage(img);
            wrote = true;
        } else if (!run.text.isEmpty()) {
            QTextCharFormat fmt = charFormatFromStyle(run.style);
            if (!run.footnoteId.isEmpty()) {
                fmt.setProperty(ReviewNotes::FootnoteIdProperty, run.footnoteId);
                fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
            }
            if (!run.endnoteId.isEmpty()) {
                fmt.setProperty(ReviewNotes::EndnoteIdProperty, run.endnoteId);
                fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                fmt.setForeground(QColor(120, 60, 20));
            }
            if (!run.commentId.isEmpty()) {
                fmt.setProperty(ReviewNotes::CommentIdProperty, run.commentId);
                if (!fmt.hasProperty(QTextFormat::BackgroundBrush)
                    || fmt.background().style() == Qt::NoBrush)
                    fmt.setBackground(QColor(255, 249, 196));
            }
            cursor->insertText(run.text, fmt);
            wrote = true;
        }
    }
    if (!wrote) {
        // Keep empty paragraph.
    }
}

void insertTable(QTextCursor *cursor, const DocTable &table, int *imageCounter)
{
    if (!cursor || table.columnCount <= 0 || table.rows.isEmpty())
        return;

    QTextTableFormat tf;
    tf.setBorder(table.borderPt);
    tf.setCellPadding(table.cellPaddingPt);
    tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    if (table.columnWeights.size() == table.columnCount && table.columnCount > 0) {
        qreal sum = 0;
        for (qreal w : table.columnWeights)
            sum += qMax(0.0, w);
        if (sum > 0) {
            QList<QTextLength> constraints;
            for (qreal w : table.columnWeights) {
                constraints << QTextLength(QTextLength::PercentageLength,
                                          qMax(0.1, w) * 100.0 / sum);
            }
            tf.setColumnWidthConstraints(constraints);
        }
    }
    QTextTable *qtTable = cursor->insertTable(table.rows.size(), table.columnCount, tf);
    if (!qtTable)
        return;

    for (int r = 0; r < table.rows.size(); ++r) {
        for (int c = 0; c < table.columnCount; ++c) {
            if (c >= table.rows[r].size())
                continue;
            const DocTableCell &cell = table.rows[r][c];
            if (cell.covered)
                continue;
            QTextTableCell qtCell = qtTable->cellAt(r, c);
            QTextCursor cellCursor = qtCell.firstCursorPosition();
            if (cell.background.isValid()) {
                QTextCharFormat cellFmt = qtCell.format();
                cellFmt.setBackground(cell.background);
                qtCell.setFormat(cellFmt);
            }
            bool first = true;
            for (const DocParagraph &para : cell.paragraphs) {
                if (!first)
                    cellCursor.insertBlock();
                first = false;
                insertParagraph(&cellCursor, para, imageCounter);
            }
        }
        if (r < table.rowMinHeightsPt.size() && table.rowMinHeightsPt.at(r) > 0.5)
            TableGeometry::setRowMinHeightPt(qtTable, r, table.rowMinHeightsPt.at(r));
    }

    // Apply merges after content is filled (Qt requires valid cells).
    for (int r = 0; r < table.rows.size(); ++r) {
        for (int c = 0; c < table.columnCount; ++c) {
            if (c >= table.rows[r].size())
                continue;
            const DocTableCell &cell = table.rows[r][c];
            if (cell.covered)
                continue;
            const int rs = qMax(1, cell.rowSpan);
            const int cs = qMax(1, cell.columnSpan);
            if (rs > 1 || cs > 1)
                qtTable->mergeCells(r, c, rs, cs);
        }
    }
    // Move cursor after the table.
    cursor->setPosition(qtTable->lastPosition() + 1);
}

} // namespace

void toDocument(const DocumentModel &model, QTextDocument *document)
{
    if (!document)
        return;

    document->clear();
    QTextCursor cursor(document);
    cursor.beginEditBlock();

    int imageCounter = 0;
    bool firstBlock = true;
    for (const DocSection &section : model.sections) {
        for (const DocBlock &block : section.blocks) {
            if (block.kind == DocBlock::Kind::Table) {
                if (!firstBlock)
                    cursor.insertBlock();
                firstBlock = false;
                insertTable(&cursor, block.table, &imageCounter);
            } else {
                if (!firstBlock)
                    cursor.insertBlock();
                firstBlock = false;
                insertParagraph(&cursor, block.paragraph, &imageCounter);
            }
        }
    }

    cursor.endEditBlock();

    if (!model.footnoteBodies.isEmpty()) {
        QVariantMap map;
        for (auto it = model.footnoteBodies.begin(); it != model.footnoteBodies.end(); ++it)
            map.insert(it.key(), it.value());
        document->addResource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://footnotes")),
                              map);
        ReviewNotes::ensureFootnotesAppendix(document);
    }

    if (!model.endnoteBodies.isEmpty()) {
        QVariantMap map;
        for (auto it = model.endnoteBodies.begin(); it != model.endnoteBodies.end(); ++it)
            map.insert(it.key(), it.value());
        document->addResource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://endnotes")),
                              map);
        ReviewNotes::ensureEndnotesAppendix(document);
    }

    if (!model.comments.isEmpty()) {
        QVariantMap map;
        for (auto it = model.comments.begin(); it != model.comments.end(); ++it) {
            QVariantMap entry;
            entry.insert(QStringLiteral("author"), it.value().author);
            entry.insert(QStringLiteral("text"), it.value().text);
            entry.insert(QStringLiteral("start"), -1);
            entry.insert(QStringLiteral("end"), -1);
            map.insert(it.key(), entry);
        }
        // Refresh start/end from live markers so ReviewNotes::collectComments stays accurate.
        for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !(it.atEnd()); ++it) {
                const QTextFragment frag = it.fragment();
                if (!frag.isValid())
                    continue;
                const QTextCharFormat fmt = frag.charFormat();
                if (!fmt.hasProperty(ReviewNotes::CommentIdProperty))
                    continue;
                const QString id = fmt.property(ReviewNotes::CommentIdProperty).toString();
                if (!map.contains(id))
                    continue;
                QVariantMap entry = map.value(id).toMap();
                const int start = frag.position();
                const int end = start + frag.length();
                const int prevStart = entry.value(QStringLiteral("start")).toInt();
                const int prevEnd = entry.value(QStringLiteral("end")).toInt();
                if (prevStart < 0 || start < prevStart)
                    entry.insert(QStringLiteral("start"), start);
                if (prevEnd < 0 || end > prevEnd)
                    entry.insert(QStringLiteral("end"), end);
                map.insert(id, entry);
            }
        }
        document->addResource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://comments")),
                              map);
    }

    if (!model.floatingBoxes.isEmpty())
        FloatingTextBoxes::save(document, model.floatingBoxes, false);

    document->setModified(false);
}

void SnapshotCache::invalidate()
{
    m_model = DocumentModel{};
    m_revision = -1;
    m_valid = false;
    m_hasDirtyHint = false;
    m_dirtyPos = 0;
}

bool SnapshotCache::matches(int documentRevision, const PageLayoutSettings &setup) const
{
    return m_valid && documentRevision == m_revision && setupsEqual(m_setup, setup);
}

void SnapshotCache::noteChange(int position)
{
    if (position < 0)
        position = 0;
    if (!m_hasDirtyHint) {
        m_dirtyPos = position;
        m_hasDirtyHint = true;
    } else {
        m_dirtyPos = qMin(m_dirtyPos, position);
    }
}

bool SnapshotCache::pageSetupEquals(const PageLayoutSettings &a, const PageLayoutSettings &b) const
{
    return setupsEqual(a, b);
}

DocumentModel SnapshotCache::fullRebuild(const QTextDocument *document,
                                         const PageLayoutSettings &pageSetup)
{
    m_model = buildModel(document, pageSetup, 0, {});
    m_setup = pageSetup;
    m_revision = document ? document->revision() : -1;
    m_valid = document != nullptr;
    m_hasDirtyHint = false;
    m_dirtyPos = 0;
    return m_model;
}

DocumentModel SnapshotCache::suffixRebuild(const QTextDocument *document,
                                           const PageLayoutSettings &pageSetup,
                                           int fromPos)
{
    if (!document || m_model.sections.isEmpty())
        return fullRebuild(document, pageSetup);

    // Note/comment metadata is document-global — prefer a full rebuild when present.
    const QVariantMap fnMap =
        document->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://footnotes")))
            .toMap();
    const QVariantMap enMap =
        document->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://endnotes")))
            .toMap();
    const QVariantMap cmMap =
        document->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://comments")))
            .toMap();
    if (!fnMap.isEmpty() || !m_model.footnoteOrder.isEmpty() || !enMap.isEmpty()
        || !m_model.endnoteOrder.isEmpty() || !cmMap.isEmpty() || !m_model.commentOrder.isEmpty())
        return fullRebuild(document, pageSetup);

    QVector<DocBlock> &blocks = m_model.sections[0].blocks;
    int rebuildIdx = 0;
    for (; rebuildIdx < blocks.size(); ++rebuildIdx) {
        const int start = blocks[rebuildIdx].documentPosition;
        const int end = (rebuildIdx + 1 < blocks.size())
            ? blocks[rebuildIdx + 1].documentPosition
            : std::numeric_limits<int>::max();
        if (fromPos < end) {
            if (fromPos <= start && rebuildIdx > 0)
                --rebuildIdx;
            break;
        }
    }
    if (rebuildIdx >= blocks.size())
        rebuildIdx = qMax(0, blocks.size() - 1);

    const int cutPos = blocks.isEmpty() ? 0 : blocks[rebuildIdx].documentPosition;
    blocks.resize(rebuildIdx);
    m_model.sections[0].pageSetup = pageSetup;

    AdaptContext ctx;
    ctx.model = &m_model;
    appendFromFrame(document->rootFrame(), &m_model.sections[0], document, document->defaultFont(),
                    cutPos, &ctx);

    if (m_model.sections[0].blocks.isEmpty()) {
        DocBlock block;
        block.kind = DocBlock::Kind::Paragraph;
        DocRun empty;
        empty.style.font = document->defaultFont();
        if (empty.style.font.pointSizeF() <= 0)
            empty.style.font.setPointSizeF(12.0);
        block.paragraph.runs.append(empty);
        m_model.sections[0].blocks.append(block);
    }

    m_setup = pageSetup;
    m_revision = document->revision();
    m_valid = true;
    m_hasDirtyHint = false;
    m_dirtyPos = 0;
    m_model.floatingBoxes = FloatingTextBoxes::load(document);
    return m_model;
}

DocumentModel SnapshotCache::ensure(const QTextDocument *document,
                                    const PageLayoutSettings &pageSetup)
{
    if (!document)
        return {};

    if (matches(document->revision(), pageSetup)) {
        m_hasDirtyHint = false;
        m_model.floatingBoxes = FloatingTextBoxes::load(document);
        return m_model;
    }

    if (!m_valid || !pageSetupEquals(m_setup, pageSetup) || m_model.sections.isEmpty())
        return fullRebuild(document, pageSetup);

    // No dirty hint (e.g. format-only / external mutation): keep it correct with a full pass.
    if (!m_hasDirtyHint)
        return fullRebuild(document, pageSetup);

    // Tiny docs: full rebuild is cheaper than prefix bookkeeping.
    if (document->characterCount() < 800 || m_model.sections[0].blocks.size() < 8)
        return fullRebuild(document, pageSetup);

    // Edits near the start still rewrite almost everything — prefer one clean full pass.
    const int cutHint = m_dirtyPos;
    if (cutHint <= 64)
        return fullRebuild(document, pageSetup);

    return suffixRebuild(document, pageSetup, cutHint);
}

} // namespace QTextAdapter
} // namespace Engine
