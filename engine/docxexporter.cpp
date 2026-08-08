#include "docxexporter.h"
#include "docxcommon.h"
#include "docxmeta.h"
#include "styleutils.h"
#include "ziparchive.h"

#include <QBuffer>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QXmlStreamWriter>

#include <algorithm>
#include <utility>

namespace Engine {
namespace DocxExporter {
namespace {

void writeEmptyAttrs(QXmlStreamWriter &w, const QString &name,
                     const QList<QPair<QString, QString>> &attrs = {})
{
    w.writeStartElement(name);
    for (const auto &a : attrs)
        w.writeAttribute(a.first, a.second);
    w.writeEndElement();
}

QString colorHex(const QColor &c)
{
    return QStringLiteral("%1%2%3")
        .arg(c.red(), 2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(), 2, 16, QLatin1Char('0'));
}

void writeRun(QXmlStreamWriter &w, const DocRun &run, const QHash<QString, int> &footnoteIds,
              const QHash<QString, int> &endnoteIds)
{
    if (run.isAtomic)
        return;

    const int fnId = (!run.footnoteId.isEmpty()) ? footnoteIds.value(run.footnoteId, -1) : -1;
    const int enId = (!run.endnoteId.isEmpty()) ? endnoteIds.value(run.endnoteId, -1) : -1;
    const bool isFootnote = fnId > 0;
    const bool isEndnote = enId > 0;
    if (!isFootnote && !isEndnote && run.text.isEmpty())
        return;

    w.writeStartElement(QStringLiteral("w:r"));
    w.writeStartElement(QStringLiteral("w:rPr"));
    if (run.style.bold || run.style.font.bold())
        writeEmptyAttrs(w, QStringLiteral("w:b"));
    if (run.style.italic || run.style.font.italic())
        writeEmptyAttrs(w, QStringLiteral("w:i"));
    if (run.style.underline || run.style.font.underline())
        writeEmptyAttrs(w, QStringLiteral("w:u"),
                        {{QStringLiteral("w:val"), QStringLiteral("single")}});
    const qreal pt = run.style.font.pointSizeF();
    if (pt > 0) {
        const QString sz = QString::number(int(pt * 2));
        writeEmptyAttrs(w, QStringLiteral("w:sz"), {{QStringLiteral("w:val"), sz}});
        writeEmptyAttrs(w, QStringLiteral("w:szCs"), {{QStringLiteral("w:val"), sz}});
    }
    const QString family = run.style.font.family();
    if (!family.isEmpty()) {
        writeEmptyAttrs(w, QStringLiteral("w:rFonts"),
                        {{QStringLiteral("w:ascii"), family},
                         {QStringLiteral("w:hAnsi"), family},
                         {QStringLiteral("w:eastAsia"), family}});
    }
    if (run.style.foreground.isValid() && run.style.foreground != QColor(Qt::black)) {
        writeEmptyAttrs(w, QStringLiteral("w:color"),
                        {{QStringLiteral("w:val"), colorHex(run.style.foreground)}});
    }
    if (run.style.background.isValid()) {
        writeEmptyAttrs(w, QStringLiteral("w:shd"),
                        {{QStringLiteral("w:val"), QStringLiteral("clear")},
                         {QStringLiteral("w:color"), QStringLiteral("auto")},
                         {QStringLiteral("w:fill"), colorHex(run.style.background)}});
    }
    if (isFootnote || isEndnote || run.style.superscript) {
        writeEmptyAttrs(w, QStringLiteral("w:vertAlign"),
                        {{QStringLiteral("w:val"), QStringLiteral("superscript")}});
    }
    w.writeEndElement(); // rPr

    if (isFootnote) {
        writeEmptyAttrs(w, QStringLiteral("w:footnoteReference"),
                        {{QStringLiteral("w:id"), QString::number(fnId)}});
    } else if (isEndnote) {
        writeEmptyAttrs(w, QStringLiteral("w:endnoteReference"),
                        {{QStringLiteral("w:id"), QString::number(enId)}});
    } else {
        const QStringList parts = run.text.split(QChar::LineSeparator);
        for (int i = 0; i < parts.size(); ++i) {
            if (i > 0)
                writeEmptyAttrs(w, QStringLiteral("w:br"));
            if (parts[i].isEmpty())
                continue;
            w.writeStartElement(QStringLiteral("w:t"));
            w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
            w.writeCharacters(parts[i]);
            w.writeEndElement();
        }
    }
    w.writeEndElement(); // r
}

void writeImageRun(QXmlStreamWriter &w, int relId, qreal widthPt, qreal heightPt, int &docPrId)
{
    const qreal wPt = widthPt > 0 ? widthPt : 240;
    const qreal hPt = heightPt > 0 ? heightPt : 180;
    const qint64 cx = qint64(wPt * 12700); // 1pt = 12700 EMU
    const qint64 cy = qint64(hPt * 12700);
    ++docPrId;

    w.writeStartElement(QStringLiteral("w:r"));
    w.writeStartElement(QStringLiteral("w:drawing"));
    w.writeStartElement(QStringLiteral("wp:inline"));
    w.writeAttribute(QStringLiteral("xmlns:wp"),
                     QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"));
    w.writeStartElement(QStringLiteral("wp:extent"));
    w.writeAttribute(QStringLiteral("cx"), QString::number(cx));
    w.writeAttribute(QStringLiteral("cy"), QString::number(cy));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("wp:docPr"));
    w.writeAttribute(QStringLiteral("id"), QString::number(docPrId));
    w.writeAttribute(QStringLiteral("name"), QStringLiteral("Picture %1").arg(docPrId));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("a:graphic"));
    w.writeAttribute(QStringLiteral("xmlns:a"),
                     QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main"));
    w.writeStartElement(QStringLiteral("a:graphicData"));
    w.writeAttribute(QStringLiteral("uri"),
                     QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/picture"));
    w.writeStartElement(QStringLiteral("pic:pic"));
    w.writeAttribute(QStringLiteral("xmlns:pic"),
                     QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/picture"));
    w.writeStartElement(QStringLiteral("pic:nvPicPr"));
    w.writeStartElement(QStringLiteral("pic:cNvPr"));
    w.writeAttribute(QStringLiteral("id"), QString::number(docPrId));
    w.writeAttribute(QStringLiteral("name"), QStringLiteral("image%1.png").arg(relId));
    w.writeEndElement();
    writeEmptyAttrs(w, QStringLiteral("pic:cNvPicPr"));
    w.writeEndElement(); // nvPicPr
    w.writeStartElement(QStringLiteral("pic:blipFill"));
    w.writeStartElement(QStringLiteral("a:blip"));
    w.writeAttribute(QStringLiteral("xmlns:r"),
                     QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"));
    w.writeAttribute(QStringLiteral("r:embed"), QStringLiteral("rId%1").arg(relId));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("a:stretch"));
    writeEmptyAttrs(w, QStringLiteral("a:fillRect"));
    w.writeEndElement();
    w.writeEndElement(); // blipFill
    w.writeStartElement(QStringLiteral("pic:spPr"));
    w.writeStartElement(QStringLiteral("a:xfrm"));
    w.writeStartElement(QStringLiteral("a:off"));
    w.writeAttribute(QStringLiteral("x"), QStringLiteral("0"));
    w.writeAttribute(QStringLiteral("y"), QStringLiteral("0"));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("a:ext"));
    w.writeAttribute(QStringLiteral("cx"), QString::number(cx));
    w.writeAttribute(QStringLiteral("cy"), QString::number(cy));
    w.writeEndElement();
    w.writeEndElement(); // xfrm
    w.writeStartElement(QStringLiteral("a:prstGeom"));
    w.writeAttribute(QStringLiteral("prst"), QStringLiteral("rect"));
    writeEmptyAttrs(w, QStringLiteral("a:avLst"));
    w.writeEndElement();
    w.writeEndElement(); // spPr
    w.writeEndElement(); // pic
    w.writeEndElement(); // graphicData
    w.writeEndElement(); // graphic
    w.writeEndElement(); // inline
    w.writeEndElement(); // drawing
    w.writeEndElement(); // r
}

void writeParagraph(QXmlStreamWriter &w,
                    const DocParagraph &para,
                    const QHash<const DocRun *, int> &imageRels,
                    const QHash<QString, int> &footnoteIds,
                    const QHash<QString, int> &endnoteIds,
                    const QHash<QString, int> &commentIds,
                    int &docPrId)
{
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:pPr"));
    writeEmptyAttrs(w, QStringLiteral("w:pStyle"),
                    {{QStringLiteral("w:val"), DocxCommon::styleIdToDocx(para.styleId)}});
    if (para.pageBreakBefore)
        writeEmptyAttrs(w, QStringLiteral("w:pageBreakBefore"));
    w.writeEndElement(); // pPr

    auto closeComment = [&](const QString &id) {
        const int cid = commentIds.value(id, -1);
        if (cid < 0)
            return;
        writeEmptyAttrs(w, QStringLiteral("w:commentRangeEnd"),
                        {{QStringLiteral("w:id"), QString::number(cid)}});
        w.writeStartElement(QStringLiteral("w:r"));
        writeEmptyAttrs(w, QStringLiteral("w:commentReference"),
                        {{QStringLiteral("w:id"), QString::number(cid)}});
        w.writeEndElement();
    };
    auto openComment = [&](const QString &id) {
        const int cid = commentIds.value(id, -1);
        if (cid < 0)
            return;
        writeEmptyAttrs(w, QStringLiteral("w:commentRangeStart"),
                        {{QStringLiteral("w:id"), QString::number(cid)}});
    };

    bool wrote = false;
    QString openCommentId;
    for (const DocRun &run : para.runs) {
        if (run.commentId != openCommentId) {
            if (!openCommentId.isEmpty())
                closeComment(openCommentId);
            openCommentId = run.commentId;
            if (!openCommentId.isEmpty())
                openComment(openCommentId);
        }
        if (run.isAtomic) {
            const int relId = imageRels.value(&run, -1);
            if (relId > 0) {
                writeImageRun(w, relId, run.atomicWidthPt, run.atomicHeightPt, docPrId);
                wrote = true;
            }
        } else if (!run.text.isEmpty() || !run.footnoteId.isEmpty() || !run.endnoteId.isEmpty()) {
            writeRun(w, run, footnoteIds, endnoteIds);
            wrote = true;
        }
    }
    if (!openCommentId.isEmpty())
        closeComment(openCommentId);
    if (!wrote) {
        // Empty paragraph still valid in OOXML.
    }
    w.writeEndElement(); // p
}

void writeTable(QXmlStreamWriter &w,
                const DocTable &table,
                const QHash<const DocRun *, int> &imageRels,
                const QHash<QString, int> &footnoteIds,
                const QHash<QString, int> &endnoteIds,
                const QHash<QString, int> &commentIds,
                int &docPrId)
{
    w.writeStartElement(QStringLiteral("w:tbl"));
    w.writeStartElement(QStringLiteral("w:tblPr"));
    w.writeStartElement(QStringLiteral("w:tblW"));
    w.writeAttribute(QStringLiteral("w:w"), QStringLiteral("0"));
    w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("auto"));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("w:tblBorders"));
    for (const char *edge : {"top", "left", "bottom", "right", "insideH", "insideV"}) {
        w.writeStartElement(QStringLiteral("w:") + QLatin1String(edge));
        w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("single"));
        w.writeAttribute(QStringLiteral("w:sz"), QStringLiteral("4"));
        w.writeAttribute(QStringLiteral("w:color"), QStringLiteral("000000"));
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndElement(); // tblPr

    w.writeStartElement(QStringLiteral("w:tblGrid"));
    {
        qreal sum = 0;
        QVector<qreal> weights = table.columnWeights;
        if (weights.size() != table.columnCount)
            weights = QVector<qreal>(table.columnCount, 1.0);
        for (qreal wgt : weights)
            sum += qMax(0.01, wgt);
        const int totalTwips = 9000;
        for (int c = 0; c < table.columnCount; ++c) {
            const int tw = qMax(200, qRound(totalTwips * qMax(0.01, weights[c]) / sum));
            writeEmptyAttrs(w, QStringLiteral("w:gridCol"),
                            {{QStringLiteral("w:w"), QString::number(tw)}});
        }
    }
    w.writeEndElement();

    QVector<qreal> weights = table.columnWeights;
    if (weights.size() != table.columnCount)
        weights = QVector<qreal>(table.columnCount, 1.0);
    qreal weightSum = 0;
    for (qreal wgt : weights)
        weightSum += qMax(0.01, wgt);
    if (weightSum < 0.01)
        weightSum = 1.0;

    auto spanWidthTwips = [&](int startCol, int colSpan) {
        qreal part = 0;
        for (int k = 0; k < colSpan && startCol + k < table.columnCount; ++k)
            part += qMax(0.01, weights[startCol + k]);
        return qMax(200, qRound(9000 * part / weightSum));
    };

    auto findVerticalAnchor = [&](int r, int c) -> const DocTableCell * {
        // Left edge of a vertical merge is always at the anchor's start column.
        for (int ar = r - 1; ar >= 0; --ar) {
            if (c >= table.rows[ar].size())
                return nullptr;
            const DocTableCell &cand = table.rows[ar][c];
            if (cand.covered)
                continue;
            const int rs = qMax(1, cand.rowSpan);
            if (rs > 1 && ar + rs > r)
                return &cand;
            return nullptr;
        }
        return nullptr;
    };

    for (int r = 0; r < table.rows.size(); ++r) {
        const auto &row = table.rows.at(r);
        w.writeStartElement(QStringLiteral("w:tr"));
        if (r < table.rowMinHeightsPt.size() && table.rowMinHeightsPt.at(r) > 0.5) {
            const int twips = qMax(20, qRound(table.rowMinHeightsPt.at(r) * 20.0));
            w.writeStartElement(QStringLiteral("w:trPr"));
            writeEmptyAttrs(w, QStringLiteral("w:trHeight"),
                            {{QStringLiteral("w:val"), QString::number(twips)},
                             {QStringLiteral("w:hRule"), QStringLiteral("atLeast")}});
            w.writeEndElement(); // trPr
        }
        for (int c = 0; c < table.columnCount; ) {
            const DocTableCell *cellPtr = (c < row.size()) ? &row[c] : nullptr;
            const bool covered = cellPtr && cellPtr->covered;
            int colSpan = 1;
            int rowSpan = 1;
            bool vContinue = false;
            const DocTableCell *writeCell = cellPtr;

            if (covered) {
                if (const DocTableCell *anchor = findVerticalAnchor(r, c)) {
                    colSpan = qMax(1, anchor->columnSpan);
                    vContinue = true;
                    writeCell = nullptr; // empty continue cell
                } else {
                    ++c; // horizontal cover under same-row span
                    continue;
                }
            } else if (cellPtr) {
                colSpan = qBound(1, cellPtr->columnSpan, table.columnCount - c);
                rowSpan = qMax(1, cellPtr->rowSpan);
            }

            w.writeStartElement(QStringLiteral("w:tc"));
            w.writeStartElement(QStringLiteral("w:tcPr"));
            {
                const int tw = spanWidthTwips(c, colSpan);
                w.writeStartElement(QStringLiteral("w:tcW"));
                w.writeAttribute(QStringLiteral("w:w"), QString::number(tw));
                w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("dxa"));
                w.writeEndElement();
            }
            if (colSpan > 1) {
                writeEmptyAttrs(w, QStringLiteral("w:gridSpan"),
                                {{QStringLiteral("w:val"), QString::number(colSpan)}});
            }
            if (vContinue) {
                writeEmptyAttrs(w, QStringLiteral("w:vMerge"), {});
            } else if (rowSpan > 1) {
                writeEmptyAttrs(w, QStringLiteral("w:vMerge"),
                                {{QStringLiteral("w:val"), QStringLiteral("restart")}});
            }
            if (writeCell && writeCell->background.isValid()) {
                writeEmptyAttrs(w, QStringLiteral("w:shd"),
                                {{QStringLiteral("w:val"), QStringLiteral("clear")},
                                 {QStringLiteral("w:color"), QStringLiteral("auto")},
                                 {QStringLiteral("w:fill"), colorHex(writeCell->background)}});
            }
            w.writeEndElement(); // tcPr

            bool wrotePara = false;
            if (writeCell) {
                for (const DocParagraph &para : writeCell->paragraphs) {
                    writeParagraph(w, para, imageRels, footnoteIds, endnoteIds, commentIds, docPrId);
                    wrotePara = true;
                }
            }
            if (!wrotePara) {
                w.writeStartElement(QStringLiteral("w:p"));
                w.writeEndElement();
            }
            w.writeEndElement(); // tc
            c += colSpan;
        }
        w.writeEndElement(); // tr
    }
    w.writeEndElement(); // tbl
}

QHash<QString, int> buildFootnoteIdMap(const DocumentModel &model)
{
    QHash<QString, int> map;
    int next = 1;
    for (const QString &id : model.footnoteOrder) {
        if (id.isEmpty() || map.contains(id))
            continue;
        map.insert(id, next++);
    }
    for (auto it = model.footnoteBodies.begin(); it != model.footnoteBodies.end(); ++it) {
        if (it.key().isEmpty() || map.contains(it.key()))
            continue;
        map.insert(it.key(), next++);
    }
    return map;
}

QByteArray buildFootnotesXml(const DocumentModel &model, const QHash<QString, int> &footnoteIds)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("w:footnotes"));
    w.writeAttribute(QStringLiteral("xmlns:w"),
                     QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));

    w.writeStartElement(QStringLiteral("w:footnote"));
    w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("separator"));
    w.writeAttribute(QStringLiteral("w:id"), QStringLiteral("-1"));
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:r"));
    writeEmptyAttrs(w, QStringLiteral("w:separator"));
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("w:footnote"));
    w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("continuationSeparator"));
    w.writeAttribute(QStringLiteral("w:id"), QStringLiteral("0"));
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:r"));
    writeEmptyAttrs(w, QStringLiteral("w:continuationSeparator"));
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    QVector<QPair<int, QString>> ordered; // numericId → modelId
    ordered.reserve(footnoteIds.size());
    for (auto it = footnoteIds.begin(); it != footnoteIds.end(); ++it)
        ordered.append({it.value(), it.key()});
    std::sort(ordered.begin(), ordered.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });

    for (const auto &entry : ordered) {
        const QString body = model.footnoteBodies.value(entry.second);
        w.writeStartElement(QStringLiteral("w:footnote"));
        w.writeAttribute(QStringLiteral("w:id"), QString::number(entry.first));
        w.writeStartElement(QStringLiteral("w:p"));
        w.writeStartElement(QStringLiteral("w:pPr"));
        writeEmptyAttrs(w, QStringLiteral("w:pStyle"),
                        {{QStringLiteral("w:val"), QStringLiteral("FootnoteText")}});
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:rPr"));
        writeEmptyAttrs(w, QStringLiteral("w:vertAlign"),
                        {{QStringLiteral("w:val"), QStringLiteral("superscript")}});
        w.writeEndElement();
        writeEmptyAttrs(w, QStringLiteral("w:footnoteRef"));
        w.writeEndElement(); // r
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:t"));
        w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
        w.writeCharacters(QLatin1Char(' ') + body);
        w.writeEndElement();
        w.writeEndElement(); // r
        w.writeEndElement(); // p
        w.writeEndElement(); // footnote
    }

    w.writeEndElement(); // footnotes
    w.writeEndDocument();
    return xml;
}

QHash<QString, int> buildEndnoteIdMap(const DocumentModel &model)
{
    QHash<QString, int> map;
    int next = 1;
    for (const QString &id : model.endnoteOrder) {
        if (id.isEmpty() || map.contains(id))
            continue;
        map.insert(id, next++);
    }
    for (auto it = model.endnoteBodies.begin(); it != model.endnoteBodies.end(); ++it) {
        if (it.key().isEmpty() || map.contains(it.key()))
            continue;
        map.insert(it.key(), next++);
    }
    return map;
}

QByteArray buildEndnotesXml(const DocumentModel &model, const QHash<QString, int> &endnoteIds)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("w:endnotes"));
    w.writeAttribute(QStringLiteral("xmlns:w"),
                     QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));

    w.writeStartElement(QStringLiteral("w:endnote"));
    w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("separator"));
    w.writeAttribute(QStringLiteral("w:id"), QStringLiteral("-1"));
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:r"));
    writeEmptyAttrs(w, QStringLiteral("w:separator"));
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("w:endnote"));
    w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("continuationSeparator"));
    w.writeAttribute(QStringLiteral("w:id"), QStringLiteral("0"));
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:r"));
    writeEmptyAttrs(w, QStringLiteral("w:continuationSeparator"));
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    QVector<QPair<int, QString>> ordered;
    ordered.reserve(endnoteIds.size());
    for (auto it = endnoteIds.begin(); it != endnoteIds.end(); ++it)
        ordered.append({it.value(), it.key()});
    std::sort(ordered.begin(), ordered.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });

    for (const auto &entry : ordered) {
        const QString body = model.endnoteBodies.value(entry.second);
        w.writeStartElement(QStringLiteral("w:endnote"));
        w.writeAttribute(QStringLiteral("w:id"), QString::number(entry.first));
        w.writeStartElement(QStringLiteral("w:p"));
        w.writeStartElement(QStringLiteral("w:pPr"));
        writeEmptyAttrs(w, QStringLiteral("w:pStyle"),
                        {{QStringLiteral("w:val"), QStringLiteral("EndnoteText")}});
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:rPr"));
        writeEmptyAttrs(w, QStringLiteral("w:vertAlign"),
                        {{QStringLiteral("w:val"), QStringLiteral("superscript")}});
        w.writeEndElement();
        writeEmptyAttrs(w, QStringLiteral("w:endnoteRef"));
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:t"));
        w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
        w.writeCharacters(QLatin1Char(' ') + body);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
    }

    w.writeEndElement();
    w.writeEndDocument();
    return xml;
}

QHash<QString, int> buildCommentIdMap(const DocumentModel &model)
{
    QHash<QString, int> map;
    int next = 0; // OOXML comment ids are typically 0-based
    for (const QString &id : model.commentOrder) {
        if (id.isEmpty() || map.contains(id))
            continue;
        map.insert(id, next++);
    }
    for (auto it = model.comments.begin(); it != model.comments.end(); ++it) {
        if (it.key().isEmpty() || map.contains(it.key()))
            continue;
        map.insert(it.key(), next++);
    }
    return map;
}

QByteArray buildCommentsXml(const DocumentModel &model, const QHash<QString, int> &commentIds)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("w:comments"));
    w.writeAttribute(QStringLiteral("xmlns:w"),
                     QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));

    QVector<QPair<int, QString>> ordered;
    ordered.reserve(commentIds.size());
    for (auto it = commentIds.begin(); it != commentIds.end(); ++it)
        ordered.append({it.value(), it.key()});
    std::sort(ordered.begin(), ordered.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });

    for (const auto &entry : ordered) {
        const DocComment comment = model.comments.value(entry.second);
        w.writeStartElement(QStringLiteral("w:comment"));
        w.writeAttribute(QStringLiteral("w:id"), QString::number(entry.first));
        w.writeAttribute(QStringLiteral("w:author"),
                         comment.author.isEmpty() ? QStringLiteral("NewWord") : comment.author);
        w.writeAttribute(QStringLiteral("w:initials"),
                         comment.author.isEmpty() ? QStringLiteral("NW")
                                                  : comment.author.left(2));
        w.writeStartElement(QStringLiteral("w:p"));
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:t"));
        w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
        w.writeCharacters(comment.text);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement(); // comment
    }

    w.writeEndElement(); // comments
    w.writeEndDocument();
    return xml;
}

void collectImages(const DocumentModel &model,
                   QHash<const DocRun *, int> *imageRels,
                   QList<QPair<QString, QByteArray>> *mediaFiles,
                   int *nextRel)
{
    auto addRun = [&](const DocRun &run) {
        if (!run.isAtomic || run.image.isNull())
            return;
        if (imageRels->contains(&run))
            return;
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        if (!run.image.save(&buf, "PNG"))
            return;
        const int id = (*nextRel)++;
        imageRels->insert(&run, id);
        mediaFiles->append({QStringLiteral("word/media/image%1.png").arg(id), png});
    };

    for (const DocSection &section : model.sections) {
        for (const DocBlock &block : section.blocks) {
            if (block.kind == DocBlock::Kind::Paragraph) {
                for (const DocRun &run : block.paragraph.runs)
                    addRun(run);
            } else {
                for (const auto &row : block.table.rows) {
                    for (const DocTableCell &cell : row) {
                        for (const DocParagraph &para : cell.paragraphs) {
                            for (const DocRun &run : para.runs)
                                addRun(run);
                        }
                    }
                }
            }
        }
    }
}

} // namespace

bool save(const DocumentModel &model, const QString &filePath, QString *errorMessage,
          const DocxDocumentMeta *meta)
{
    ZipWriter zip(filePath);
    if (!zip.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 DOCX 文件。");
        return false;
    }

    PageLayoutSettings pageLayout;
    if (meta && meta->writePageLayout)
        pageLayout = meta->pageLayout;
    else if (!model.sections.isEmpty())
        pageLayout = model.sections.first().pageSetup;

    auto mmToTwips = [](qreal mm) { return qRound(mm * 1440.0 / 25.4); };
    const QSizeF pageMm = pageLayout.pageSizeMm();
    const int pgW = mmToTwips(pageMm.width());
    const int pgH = mmToTwips(pageMm.height());
    const int mTop = mmToTwips(pageLayout.marginsMm.top());
    const int mBottom = mmToTwips(pageLayout.marginsMm.bottom());
    const int mLeft = mmToTwips(pageLayout.marginsMm.left());
    const int mRight = mmToTwips(pageLayout.marginsMm.right());
    const int mHeader = mmToTwips(pageLayout.headerDistanceMm);
    const int mFooter = mmToTwips(pageLayout.footerDistanceMm);

    const bool writeHf = meta && meta->writeHeaderFooter;
    int nextRel = 2; // rId1 = styles
    const int hdrRel = writeHf ? nextRel++ : -1;
    const int ftrRel = writeHf ? nextRel++ : -1;

    const QHash<QString, int> footnoteIds = buildFootnoteIdMap(model);
    const bool writeFootnotes = !footnoteIds.isEmpty();
    const int footnotesRel = writeFootnotes ? nextRel++ : -1;

    const QHash<QString, int> endnoteIds = buildEndnoteIdMap(model);
    const bool writeEndnotes = !endnoteIds.isEmpty();
    const int endnotesRel = writeEndnotes ? nextRel++ : -1;

    const QHash<QString, int> commentIds = buildCommentIdMap(model);
    const bool writeComments = !commentIds.isEmpty();
    const int commentsRel = writeComments ? nextRel++ : -1;

    QHash<const DocRun *, int> imageRels;
    QList<QPair<QString, QByteArray>> mediaFiles;
    collectImages(model, &imageRels, &mediaFiles, &nextRel);

    QByteArray documentXml;
    {
        QBuffer buffer(&documentXml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        w.setAutoFormatting(true);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(QStringLiteral("w:document"));
        w.writeAttribute(QStringLiteral("xmlns:w"),
                         QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));
        w.writeAttribute(QStringLiteral("xmlns:r"),
                         QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"));
        w.writeStartElement(QStringLiteral("w:body"));

        int docPrId = 0;
        for (const DocSection &section : model.sections) {
            for (const DocBlock &block : section.blocks) {
                if (block.kind == DocBlock::Kind::Table)
                    writeTable(w, block.table, imageRels, footnoteIds, endnoteIds, commentIds, docPrId);
                else
                    writeParagraph(w, block.paragraph, imageRels, footnoteIds, endnoteIds, commentIds,
                                   docPrId);
            }
        }

        w.writeStartElement(QStringLiteral("w:sectPr"));
        if (writeHf) {
            writeEmptyAttrs(w, QStringLiteral("w:headerReference"),
                            {{QStringLiteral("w:type"), QStringLiteral("default")},
                             {QStringLiteral("r:id"), QStringLiteral("rId%1").arg(hdrRel)}});
            writeEmptyAttrs(w, QStringLiteral("w:footerReference"),
                            {{QStringLiteral("w:type"), QStringLiteral("default")},
                             {QStringLiteral("r:id"), QStringLiteral("rId%1").arg(ftrRel)}});
        }
        w.writeStartElement(QStringLiteral("w:pgSz"));
        w.writeAttribute(QStringLiteral("w:w"), QString::number(pgW));
        w.writeAttribute(QStringLiteral("w:h"), QString::number(pgH));
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("w:pgMar"));
        w.writeAttribute(QStringLiteral("w:top"), QString::number(mTop));
        w.writeAttribute(QStringLiteral("w:right"), QString::number(mRight));
        w.writeAttribute(QStringLiteral("w:bottom"), QString::number(mBottom));
        w.writeAttribute(QStringLiteral("w:left"), QString::number(mLeft));
        w.writeAttribute(QStringLiteral("w:header"), QString::number(mHeader));
        w.writeAttribute(QStringLiteral("w:footer"), QString::number(mFooter));
        w.writeEndElement();
        w.writeEndElement(); // sectPr

        w.writeEndElement(); // body
        w.writeEndElement(); // document
        w.writeEndDocument();
    }

    QByteArray relsXml;
    {
        QBuffer buffer(&relsXml);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buffer);
        w.setAutoFormatting(true);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(QStringLiteral("Relationships"));
        w.writeAttribute(
            QStringLiteral("xmlns"),
            QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships"));
        writeEmptyAttrs(
            w, QStringLiteral("Relationship"),
            {{QStringLiteral("Id"), QStringLiteral("rId1")},
             {QStringLiteral("Type"),
              QStringLiteral(
                  "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles")},
             {QStringLiteral("Target"), QStringLiteral("styles.xml")}});
        if (writeHf) {
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(hdrRel)},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/header")},
                 {QStringLiteral("Target"), QStringLiteral("header1.xml")}});
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(ftrRel)},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer")},
                 {QStringLiteral("Target"), QStringLiteral("footer1.xml")}});
        }
        if (writeFootnotes) {
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(footnotesRel)},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes")},
                 {QStringLiteral("Target"), QStringLiteral("footnotes.xml")}});
        }
        if (writeEndnotes) {
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(endnotesRel)},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/endnotes")},
                 {QStringLiteral("Target"), QStringLiteral("endnotes.xml")}});
        }
        if (writeComments) {
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(commentsRel)},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments")},
                 {QStringLiteral("Target"), QStringLiteral("comments.xml")}});
        }
        for (auto it = imageRels.begin(); it != imageRels.end(); ++it) {
            writeEmptyAttrs(
                w, QStringLiteral("Relationship"),
                {{QStringLiteral("Id"), QStringLiteral("rId%1").arg(it.value())},
                 {QStringLiteral("Type"),
                  QStringLiteral(
                      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image")},
                 {QStringLiteral("Target"),
                  QStringLiteral("media/image%1.png").arg(it.value())}});
        }
        w.writeEndElement();
        w.writeEndDocument();
    }

    QByteArray contentTypes =
        QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Default Extension=\"png\" ContentType=\"image/png\"/>"
            "<Override PartName=\"/word/document.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
            "<Override PartName=\"/word/styles.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>");
    if (writeHf) {
        contentTypes +=
            "<Override PartName=\"/word/header1.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/>"
            "<Override PartName=\"/word/footer1.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml\"/>";
    }
    if (writeFootnotes) {
        contentTypes +=
            "<Override PartName=\"/word/footnotes.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml\"/>";
    }
    if (writeEndnotes) {
        contentTypes +=
            "<Override PartName=\"/word/endnotes.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.endnotes+xml\"/>";
    }
    if (writeComments) {
        contentTypes +=
            "<Override PartName=\"/word/comments.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.comments+xml\"/>";
    }
    contentTypes += "</Types>";

    const QByteArray rootRels = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/>"
        "</Relationships>");

    const QByteArray stylesXml = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:style w:type=\"paragraph\" w:styleId=\"Normal\" w:default=\"1\">"
        "<w:name w:val=\"Normal\"/><w:qFormat/>"
        "</w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\">"
        "<w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/>"
        "<w:pPr><w:outlineLvl w:val=\"0\"/></w:pPr>"
        "<w:rPr><w:b/><w:sz w:val=\"48\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\">"
        "<w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/>"
        "<w:pPr><w:outlineLvl w:val=\"1\"/></w:pPr>"
        "<w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading3\">"
        "<w:name w:val=\"heading 3\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/>"
        "<w:pPr><w:outlineLvl w:val=\"2\"/></w:pPr>"
        "<w:rPr><w:b/><w:sz w:val=\"28\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading4\">"
        "<w:name w:val=\"heading 4\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/>"
        "<w:pPr><w:outlineLvl w:val=\"3\"/></w:pPr>"
        "<w:rPr><w:b/><w:i/><w:sz w:val=\"26\"/></w:rPr></w:style>"
        "</w:styles>");

    zip.addFile(QStringLiteral("[Content_Types].xml"), contentTypes);
    zip.addFile(QStringLiteral("_rels/.rels"), rootRels);
    zip.addFile(QStringLiteral("word/document.xml"), documentXml);
    zip.addFile(QStringLiteral("word/_rels/document.xml.rels"), relsXml);
    zip.addFile(QStringLiteral("word/styles.xml"), stylesXml);

    if (writeHf) {
        // Reuse DocxPackage helpers via minimal inline XML (avoid circular deps).
        auto esc = [](QString t) {
            return t.replace(QLatin1Char('&'), QLatin1String("&amp;"))
                .replace(QLatin1Char('<'), QLatin1String("&lt;"))
                .replace(QLatin1Char('>'), QLatin1String("&gt;"));
        };
        const QString h = esc(meta->headerFooter.header);
        const QString f = esc(meta->headerFooter.footer);
        QByteArray hdr =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:p><w:r><w:t xml:space=\"preserve\">";
        hdr += h.toUtf8();
        hdr += "</w:t></w:r></w:p></w:hdr>";
        QByteArray ftr =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<w:ftr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:p>";
        if (!f.isEmpty()) {
            ftr += "<w:r><w:t xml:space=\"preserve\">";
            ftr += f.toUtf8();
            ftr += "</w:t></w:r>";
        }
        if (meta->headerFooter.showPageNumber) {
            ftr += "<w:r><w:t xml:space=\"preserve\">  </w:t></w:r>"
                   "<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
                   "<w:r><w:instrText xml:space=\"preserve\"> PAGE </w:instrText></w:r>"
                   "<w:r><w:fldChar w:fldCharType=\"end\"/></w:r>";
        }
        ftr += "</w:p></w:ftr>";
        zip.addFile(QStringLiteral("word/header1.xml"), hdr);
        zip.addFile(QStringLiteral("word/footer1.xml"), ftr);
    }

    if (writeFootnotes)
        zip.addFile(QStringLiteral("word/footnotes.xml"), buildFootnotesXml(model, footnoteIds));
    if (writeEndnotes)
        zip.addFile(QStringLiteral("word/endnotes.xml"), buildEndnotesXml(model, endnoteIds));
    if (writeComments)
        zip.addFile(QStringLiteral("word/comments.xml"), buildCommentsXml(model, commentIds));

    for (const auto &media : mediaFiles)
        zip.addFile(media.first, media.second);

    if (!zip.close()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写入 DOCX 失败。");
        return false;
    }
    return true;
}

} // namespace DocxExporter
} // namespace Engine
