#include "docximporter.h"
#include "docxcommon.h"
#include "docxpackage.h"
#include "styleutils.h"
#include "ziparchive.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QXmlStreamReader>

namespace Engine {
namespace DocxImporter {
namespace {

QString resolveRelTarget(const QString &baseDir, const QString &target)
{
    QString t = target;
    if (t.startsWith(QLatin1Char('/')))
        return t.mid(1);
    QString dir = baseDir;
    while (t.startsWith(QLatin1String("../"))) {
        t = t.mid(3);
        const int slash = dir.lastIndexOf(QLatin1Char('/'));
        dir = slash >= 0 ? dir.left(slash) : QString();
    }
    if (dir.isEmpty())
        return t;
    return dir + QLatin1Char('/') + t;
}

struct RelInfo {
    QString target;
    QString type;
};

QHash<QString, RelInfo> parseRelationships(const QByteArray &xml, const QString &baseDir)
{
    QHash<QString, RelInfo> map;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("Relationship"))
            continue;
        const QString id = reader.attributes().value(QLatin1String("Id")).toString();
        map.insert(id, RelInfo{
            .target = resolveRelTarget(
                baseDir, reader.attributes().value(QLatin1String("Target")).toString()),
            .type = reader.attributes().value(QLatin1String("Type")).toString(),
        });
    }
    return map;
}

QString attrVal(const QXmlStreamReader &reader, const QLatin1String &local)
{
    QString v = reader.attributes().value(local).toString();
    if (!v.isEmpty())
        return v;
    // DocxExporter writes qualified names like "w:val".
    return reader.attributes().value(QStringLiteral("w:") + local).toString();
}

bool onVal(const QXmlStreamReader &reader)
{
    const QString v = attrVal(reader, QLatin1String("val"));
    return v != QLatin1String("0") && v != QLatin1String("false");
}

QColor parseHexColor(const QString &hex)
{
    QString h = hex.trimmed();
    if (h.startsWith(QLatin1Char('#')))
        h = h.mid(1);
    if (h.size() == 6) {
        bool ok = false;
        const int rgb = h.toInt(&ok, 16);
        if (ok)
            return QColor((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    }
    return {};
}

struct RunProps {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool superscript = false;
    qreal pointSize = 0;
    QColor color;
    QColor background;
    QString fontFamily;
};

RunProps readRunProperties(QXmlStreamReader &reader)
{
    RunProps rp;
    if (!reader.isStartElement() || reader.name() != QLatin1String("rPr"))
        return rp;
    while (!(reader.isEndElement() && reader.name() == QLatin1String("rPr")) && !reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        const QStringView name = reader.name();
        if (name == QLatin1String("b"))
            rp.bold = onVal(reader);
        else if (name == QLatin1String("i"))
            rp.italic = onVal(reader);
        else if (name == QLatin1String("u"))
            rp.underline = true;
        else if (name == QLatin1String("sz") || name == QLatin1String("szCs")) {
            const qreal half = attrVal(reader, QLatin1String("val")).toDouble();
            if (half > 0)
                rp.pointSize = half / 2.0;
        } else if (name == QLatin1String("color")) {
            rp.color = parseHexColor(attrVal(reader, QLatin1String("val")));
        } else if (name == QLatin1String("rFonts")) {
            QString fam = attrVal(reader, QLatin1String("ascii"));
            if (fam.isEmpty())
                fam = attrVal(reader, QLatin1String("hAnsi"));
            if (fam.isEmpty())
                fam = attrVal(reader, QLatin1String("eastAsia"));
            rp.fontFamily = fam;
        } else if (name == QLatin1String("vertAlign")) {
            rp.superscript = attrVal(reader, QLatin1String("val"))
                                 .compare(QLatin1String("superscript"), Qt::CaseInsensitive)
                             == 0;
        } else if (name == QLatin1String("shd")) {
            rp.background = parseHexColor(attrVal(reader, QLatin1String("fill")));
        }
    }
    return rp;
}

struct ParaProps {
    StyleUtils::StyleId styleId = StyleUtils::StyleId::Normal;
    bool pageBreakBefore = false;
};

ParaProps readParagraphProperties(QXmlStreamReader &reader)
{
    ParaProps pp;
    if (!reader.isStartElement() || reader.name() != QLatin1String("pPr"))
        return pp;
    while (!(reader.isEndElement() && reader.name() == QLatin1String("pPr")) && !reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        if (reader.name() == QLatin1String("pStyle")) {
            pp.styleId = DocxCommon::docxStyleToId(attrVal(reader, QLatin1String("val")));
        } else if (reader.name() == QLatin1String("pageBreakBefore")) {
            pp.pageBreakBefore = onVal(reader);
        }
    }
    return pp;
}

CharStyle styleFromRunProps(const RunProps &rp)
{
    CharStyle style;
    style.bold = rp.bold;
    style.italic = rp.italic;
    style.underline = rp.underline;
    if (rp.pointSize > 0)
        style.font.setPointSizeF(rp.pointSize);
    if (!rp.fontFamily.isEmpty())
        style.font.setFamily(rp.fontFamily);
    if (rp.bold)
        style.font.setBold(true);
    if (rp.italic)
        style.font.setItalic(true);
    if (rp.underline)
        style.font.setUnderline(true);
    if (rp.color.isValid())
        style.foreground = rp.color;
    if (rp.background.isValid())
        style.background = rp.background;
    style.superscript = rp.superscript;
    return style;
}

class Importer
{
public:
    explicit Importer(ZipReader *zip)
        : m_zip(zip)
    {
    }

    bool read(DocumentModel *model, QString *errorMessage)
    {
        if (!model || !m_zip) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无效参数。");
            return false;
        }

        model->footnoteBodies.clear();
        model->footnoteOrder.clear();
        model->endnoteBodies.clear();
        model->endnoteOrder.clear();
        model->comments.clear();
        model->commentOrder.clear();

        QByteArray rels = m_zip->fileData(QStringLiteral("word/_rels/document.xml.rels"));
        m_rels = parseRelationships(rels, QStringLiteral("word"));

        loadFootnotesPart(model);
        loadEndnotesPart(model);
        loadCommentsPart(model);

        const QByteArray documentXml = m_zip->fileData(QStringLiteral("word/document.xml"));
        if (documentXml.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("DOCX 中缺少 word/document.xml。");
            return false;
        }

        DocSection section;
        parseDocument(documentXml, &section);
        if (section.blocks.isEmpty()) {
            DocBlock blank;
            blank.kind = DocBlock::Kind::Paragraph;
            DocRun empty;
            empty.style.font.setPointSizeF(12.0);
            blank.paragraph.runs.append(empty);
            section.blocks.append(blank);
        }
        model->sections.clear();
        model->sections.append(section);
        return true;
    }

private:
    void loadFootnotesPart(DocumentModel *model)
    {
        QString path;
        for (auto it = m_rels.begin(); it != m_rels.end(); ++it) {
            if (it.value().type.contains(QLatin1String("/footnotes"))) {
                path = it.value().target;
                break;
            }
        }
        if (path.isEmpty())
            path = QStringLiteral("word/footnotes.xml");

        const QByteArray xml = m_zip->fileData(path);
        if (xml.isEmpty())
            return;

        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != QLatin1String("footnote"))
                continue;
            const QString type = attrVal(reader, QLatin1String("type"));
            if (type == QLatin1String("separator")
                || type == QLatin1String("continuationSeparator")) {
                // Skip special footnotes.
                while (!(reader.isEndElement() && reader.name() == QLatin1String("footnote"))
                       && !reader.atEnd())
                    reader.readNext();
                continue;
            }
            const QString idAttr = attrVal(reader, QLatin1String("id"));
            bool ok = false;
            const int numericId = idAttr.toInt(&ok);
            if (!ok || numericId <= 0)
                continue;

            QString body;
            while (!(reader.isEndElement() && reader.name() == QLatin1String("footnote"))
                   && !reader.atEnd()) {
                reader.readNext();
                if (!reader.isStartElement())
                    continue;
                if (reader.name() == QLatin1String("t")) {
                    if (!body.isEmpty())
                        body += QLatin1Char(' ');
                    body += reader.readElementText();
                } else if (reader.name() == QLatin1String("footnoteRef")) {
                    // Marker glyph inside footnote body — skip.
                }
            }
            body = body.trimmed();
            const QString id = QString::number(numericId);
            model->footnoteBodies.insert(id, body);
            model->footnoteOrder.append(id);
        }
    }

    void loadEndnotesPart(DocumentModel *model)
    {
        QString path;
        for (auto it = m_rels.begin(); it != m_rels.end(); ++it) {
            if (it.value().type.contains(QLatin1String("/endnotes"))) {
                path = it.value().target;
                break;
            }
        }
        if (path.isEmpty())
            path = QStringLiteral("word/endnotes.xml");

        const QByteArray xml = m_zip->fileData(path);
        if (xml.isEmpty())
            return;

        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != QLatin1String("endnote"))
                continue;
            const QString type = attrVal(reader, QLatin1String("type"));
            if (type == QLatin1String("separator")
                || type == QLatin1String("continuationSeparator")) {
                while (!(reader.isEndElement() && reader.name() == QLatin1String("endnote"))
                       && !reader.atEnd())
                    reader.readNext();
                continue;
            }
            const QString idAttr = attrVal(reader, QLatin1String("id"));
            bool ok = false;
            const int numericId = idAttr.toInt(&ok);
            if (!ok || numericId <= 0)
                continue;

            QString body;
            while (!(reader.isEndElement() && reader.name() == QLatin1String("endnote"))
                   && !reader.atEnd()) {
                reader.readNext();
                if (!reader.isStartElement())
                    continue;
                if (reader.name() == QLatin1String("t")) {
                    if (!body.isEmpty())
                        body += QLatin1Char(' ');
                    body += reader.readElementText();
                }
            }
            const QString id = QString::number(numericId);
            model->endnoteBodies.insert(id, body.trimmed());
            model->endnoteOrder.append(id);
        }
    }

    void loadCommentsPart(DocumentModel *model)
    {
        QString path;
        for (auto it = m_rels.begin(); it != m_rels.end(); ++it) {
            if (it.value().type.contains(QLatin1String("/comments"))) {
                path = it.value().target;
                break;
            }
        }
        if (path.isEmpty())
            path = QStringLiteral("word/comments.xml");

        const QByteArray xml = m_zip->fileData(path);
        if (xml.isEmpty())
            return;

        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != QLatin1String("comment"))
                continue;
            const QString idAttr = attrVal(reader, QLatin1String("id"));
            const QString author = attrVal(reader, QLatin1String("author"));
            QString body;
            while (!(reader.isEndElement() && reader.name() == QLatin1String("comment"))
                   && !reader.atEnd()) {
                reader.readNext();
                if (reader.isStartElement() && reader.name() == QLatin1String("t")) {
                    if (!body.isEmpty())
                        body += QLatin1Char(' ');
                    body += reader.readElementText();
                }
            }
            const QString id = idAttr;
            DocComment comment;
            comment.author = author;
            comment.text = body.trimmed();
            model->comments.insert(id, comment);
            model->commentOrder.append(id);
        }
    }
    void parseDocument(const QByteArray &xml, DocSection *section)
    {
        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("body")) {
                while (!(reader.isEndElement() && reader.name() == QLatin1String("body"))
                       && !reader.atEnd()) {
                    reader.readNext();
                    if (!reader.isStartElement())
                        continue;
                    if (reader.name() == QLatin1String("p")) {
                        DocBlock block;
                        block.kind = DocBlock::Kind::Paragraph;
                        block.paragraph = readParagraph(reader);
                        block.documentPosition = block.paragraph.documentPosition;
                        section->blocks.append(block);
                    } else if (reader.name() == QLatin1String("tbl")) {
                        DocBlock block;
                        block.kind = DocBlock::Kind::Table;
                        block.table = readTable(reader);
                        block.documentPosition = block.table.documentPosition;
                        section->blocks.append(block);
                    } else if (reader.name() == QLatin1String("sectPr")) {
                        section->pageSetup = readSectPr(reader);
                    }
                }
            }
        }
    }

    PageLayoutSettings readSectPr(QXmlStreamReader &reader)
    {
        PageLayoutSettings setup;
        while (!(reader.isEndElement() && reader.name() == QLatin1String("sectPr"))
               && !reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("pgSz")) {
                // twips → mm (1 twip = 1/20 pt; 1 inch = 25.4 mm = 1440 twips)
                const qreal wTwip = reader.attributes().value(QLatin1String("w")).toDouble();
                const qreal hTwip = reader.attributes().value(QLatin1String("h")).toDouble();
                if (wTwip > 0 && hTwip > 0) {
                    setup.paper = PageLayoutSettings::Paper::Custom;
                    setup.customWidthMm = wTwip * 25.4 / 1440.0;
                    setup.customHeightMm = hTwip * 25.4 / 1440.0;
                }
            } else if (reader.name() == QLatin1String("pgMar")) {
                auto twipToMm = [](const QStringView &v) {
                    return v.toDouble() * 25.4 / 1440.0;
                };
                setup.marginsMm.setLeft(twipToMm(reader.attributes().value(QLatin1String("left"))));
                setup.marginsMm.setRight(twipToMm(reader.attributes().value(QLatin1String("right"))));
                setup.marginsMm.setTop(twipToMm(reader.attributes().value(QLatin1String("top"))));
                setup.marginsMm.setBottom(twipToMm(reader.attributes().value(QLatin1String("bottom"))));
            }
        }
        return setup;
    }

    DocParagraph readParagraph(QXmlStreamReader &reader)
    {
        DocParagraph para;
        ParaProps pp;
        QStringList activeComments;
        while (!(reader.isEndElement() && reader.name() == QLatin1String("p")) && !reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("pPr")) {
                pp = readParagraphProperties(reader);
            } else if (reader.name() == QLatin1String("commentRangeStart")) {
                const QString id = attrVal(reader, QLatin1String("id"));
                if (!id.isEmpty() && !activeComments.contains(id))
                    activeComments.append(id);
            } else if (reader.name() == QLatin1String("commentRangeEnd")) {
                const QString id = attrVal(reader, QLatin1String("id"));
                activeComments.removeAll(id);
            } else if (reader.name() == QLatin1String("r")) {
                const int before = para.runs.size();
                appendRunsFromElement(reader, &para);
                if (!activeComments.isEmpty()) {
                    const QString id = activeComments.last();
                    for (int i = before; i < para.runs.size(); ++i) {
                        if (para.runs[i].isAtomic)
                            continue;
                        para.runs[i].commentId = id;
                        if (!para.runs[i].style.background.isValid())
                            para.runs[i].style.background = QColor(255, 249, 196);
                    }
                }
            } else if (reader.name() == QLatin1String("hyperlink")) {
                while (!(reader.isEndElement() && reader.name() == QLatin1String("hyperlink"))
                       && !reader.atEnd()) {
                    reader.readNext();
                    if (reader.isStartElement() && reader.name() == QLatin1String("r")) {
                        const int before = para.runs.size();
                        appendRunsFromElement(reader, &para);
                        if (!activeComments.isEmpty()) {
                            const QString id = activeComments.last();
                            for (int i = before; i < para.runs.size(); ++i) {
                                if (para.runs[i].isAtomic)
                                    continue;
                                para.runs[i].commentId = id;
                                if (!para.runs[i].style.background.isValid())
                                    para.runs[i].style.background = QColor(255, 249, 196);
                            }
                        }
                    }
                }
            }
        }
        para.styleId = pp.styleId;
        para.headingLevel = StyleUtils::styleInfo(pp.styleId).headingLevel;
        para.pageBreakBefore = pp.pageBreakBefore;
        if (para.headingLevel == 1)
            para.spaceAfterPt = 12.0;
        else if (para.headingLevel >= 2)
            para.spaceAfterPt = 8.0;
        if (para.runs.isEmpty()) {
            DocRun empty;
            empty.style.font.setPointSizeF(12.0);
            para.runs.append(empty);
        }
        return para;
    }

    void appendRunsFromElement(QXmlStreamReader &reader, DocParagraph *para)
    {
        RunProps rp;
        QString text;
        auto flushText = [&]() {
            if (text.isEmpty())
                return;
            DocRun run;
            run.text = text;
            run.style = styleFromRunProps(rp);
            para->runs.append(run);
            text.clear();
        };
        while (!(reader.isEndElement() && reader.name() == QLatin1String("r")) && !reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("rPr")) {
                rp = readRunProperties(reader);
            } else if (reader.name() == QLatin1String("t")) {
                text += reader.readElementText();
            } else if (reader.name() == QLatin1String("br") || reader.name() == QLatin1String("cr")) {
                text += QChar::LineSeparator;
            } else if (reader.name() == QLatin1String("tab")) {
                text += QChar::Tabulation;
            } else if (reader.name() == QLatin1String("footnoteReference")) {
                flushText();
                const QString idAttr = attrVal(reader, QLatin1String("id"));
                bool ok = false;
                const int numericId = idAttr.toInt(&ok);
                DocRun run;
                run.footnoteId = ok && numericId > 0 ? QString::number(numericId) : idAttr;
                run.footnoteNumber = ok && numericId > 0 ? numericId : 0;
                run.text = run.footnoteNumber > 0 ? QString::number(run.footnoteNumber)
                                                  : QStringLiteral("1");
                run.style = styleFromRunProps(rp);
                run.style.superscript = true;
                para->runs.append(run);
            } else if (reader.name() == QLatin1String("endnoteReference")) {
                flushText();
                const QString idAttr = attrVal(reader, QLatin1String("id"));
                bool ok = false;
                const int numericId = idAttr.toInt(&ok);
                DocRun run;
                run.endnoteId = ok && numericId > 0 ? QString::number(numericId) : idAttr;
                run.endnoteNumber = ok && numericId > 0 ? numericId : 0;
                static const char *kRoman[] = {
                    "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x",
                    "xi", "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx"};
                if (run.endnoteNumber >= 1 && run.endnoteNumber <= 20)
                    run.text = QString::fromLatin1(kRoman[run.endnoteNumber - 1]);
                else
                    run.text = run.endnoteNumber > 0 ? QString::number(run.endnoteNumber)
                                                     : QStringLiteral("i");
                run.style = styleFromRunProps(rp);
                run.style.superscript = true;
                para->runs.append(run);
            } else if (reader.name() == QLatin1String("commentReference")) {
                // Balloon marker — range already applied via commentRangeStart/End.
                flushText();
            } else if (reader.name() == QLatin1String("drawing")
                       || reader.name() == QLatin1String("pict")) {
                flushText();
                if (DocRun img = readDrawing(reader); img.isAtomic)
                    para->runs.append(img);
            }
        }
        flushText();
    }

    DocRun readDrawing(QXmlStreamReader &reader)
    {
        DocRun run;
        const QString endName = reader.name().toString();
        QString embedId;
        qreal cx = 0;
        qreal cy = 0;
        while (!(reader.isEndElement() && reader.name() == endName) && !reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("blip")) {
                embedId = reader.attributes()
                              .value(QStringLiteral(
                                         "http://schemas.openxmlformats.org/officeDocument/2006/relationships"),
                                     QLatin1String("embed"))
                              .toString();
                if (embedId.isEmpty())
                    embedId = reader.attributes().value(QLatin1String("embed")).toString();
            } else if (reader.name() == QLatin1String("ext") && cx <= 0) {
                cx = reader.attributes().value(QLatin1String("cx")).toDouble();
                cy = reader.attributes().value(QLatin1String("cy")).toDouble();
            } else if (reader.name() == QLatin1String("extent") && cx <= 0) {
                cx = reader.attributes().value(QLatin1String("cx")).toDouble();
                cy = reader.attributes().value(QLatin1String("cy")).toDouble();
            }
        }
        if (embedId.isEmpty() || !m_rels.contains(embedId) || !m_zip)
            return run;
        const QByteArray data = m_zip->fileData(m_rels.value(embedId).target);
        QImage image;
        if (!image.loadFromData(data))
            return run;
        run.isAtomic = true;
        run.image = image;
        run.text = QStringLiteral("[image]");
        run.imageWrap = 1; // block
        // EMU → pt (914400 EMU/inch, 72 pt/inch)
        if (cx > 0)
            run.atomicWidthPt = cx / 12700.0;
        if (cy > 0)
            run.atomicHeightPt = cy / 12700.0;
        if (run.atomicWidthPt <= 0)
            run.atomicWidthPt = image.width() * 0.75;
        if (run.atomicHeightPt <= 0)
            run.atomicHeightPt = image.height() * 0.75;
        return run;
    }

    DocTable readTable(QXmlStreamReader &reader)
    {
        DocTable table;
        QVector<qreal> gridWidths;
        // 0=none, 1=vMerge restart, 2=vMerge continue (parallel to placed grid)
        QVector<QVector<quint8>> vMergeFlags;

        auto ensureBlankParagraphs = [](DocTableCell &cell) {
            if (!cell.paragraphs.isEmpty())
                return;
            DocParagraph blank;
            DocRun empty;
            empty.style.font.setPointSizeF(12.0);
            blank.runs.append(empty);
            cell.paragraphs.append(blank);
        };

        auto makeCovered = []() {
            DocTableCell covered;
            covered.covered = true;
            covered.columnSpan = 1;
            covered.rowSpan = 1;
            return covered;
        };

        while (!(reader.isEndElement() && reader.name() == QLatin1String("tbl")) && !reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("tblGrid")) {
                while (!(reader.isEndElement() && reader.name() == QLatin1String("tblGrid"))
                       && !reader.atEnd()) {
                    reader.readNext();
                    if (reader.isStartElement() && reader.name() == QLatin1String("gridCol")) {
                        bool ok = false;
                        const qreal w = attrVal(reader, QLatin1String("w")).toDouble(&ok);
                        gridWidths.append(ok && w > 0 ? w : 1.0);
                    }
                }
                continue;
            }
            if (reader.name() != QLatin1String("tr"))
                continue;

            QVector<DocTableCell> row;
            QVector<quint8> rowFlags;
            qreal rowMinPt = 0;
            int col = 0;
            int maxCol = qMax(table.columnCount, gridWidths.size());

            while (!(reader.isEndElement() && reader.name() == QLatin1String("tr"))
                   && !reader.atEnd()) {
                reader.readNext();
                if (!reader.isStartElement())
                    continue;
                if (reader.name() == QLatin1String("trPr")) {
                    while (!(reader.isEndElement() && reader.name() == QLatin1String("trPr"))
                           && !reader.atEnd()) {
                        reader.readNext();
                        if (reader.isStartElement() && reader.name() == QLatin1String("trHeight")) {
                            bool ok = false;
                            const qreal twips = attrVal(reader, QLatin1String("val")).toDouble(&ok);
                            if (ok && twips > 0)
                                rowMinPt = twips / 20.0; // twips → pt
                        }
                    }
                    continue;
                }
                if (reader.name() != QLatin1String("tc"))
                    continue;

                DocTableCell cell;
                int gridSpan = 1;
                quint8 vFlag = 0; // 0 none, 1 restart, 2 continue
                while (!(reader.isEndElement() && reader.name() == QLatin1String("tc"))
                       && !reader.atEnd()) {
                    reader.readNext();
                    if (!reader.isStartElement())
                        continue;
                    if (reader.name() == QLatin1String("tcPr")) {
                        while (!(reader.isEndElement() && reader.name() == QLatin1String("tcPr"))
                               && !reader.atEnd()) {
                            reader.readNext();
                            if (!reader.isStartElement())
                                continue;
                            if (reader.name() == QLatin1String("shd")) {
                                cell.background = parseHexColor(attrVal(reader, QLatin1String("fill")));
                            } else if (reader.name() == QLatin1String("gridSpan")) {
                                bool ok = false;
                                const int n = attrVal(reader, QLatin1String("val")).toInt(&ok);
                                if (ok && n > 1)
                                    gridSpan = n;
                            } else if (reader.name() == QLatin1String("vMerge")) {
                                const QString val = attrVal(reader, QLatin1String("val")).toLower();
                                if (val.isEmpty() || val == QLatin1String("continue"))
                                    vFlag = 2;
                                else if (val == QLatin1String("restart"))
                                    vFlag = 1;
                                else
                                    vFlag = 1; // unknown → treat as restart
                            }
                        }
                    } else if (reader.name() == QLatin1String("p")) {
                        cell.paragraphs.append(readParagraph(reader));
                    }
                }

                cell.columnSpan = qMax(1, gridSpan);
                cell.rowSpan = 1;
                if (vFlag == 2) {
                    cell.covered = true;
                } else {
                    cell.covered = false;
                    ensureBlankParagraphs(cell);
                }

                while (row.size() < col) {
                    row.append(makeCovered());
                    rowFlags.append(0);
                }
                row.append(cell);
                rowFlags.append(vFlag);
                for (int k = 1; k < cell.columnSpan; ++k) {
                    row.append(makeCovered());
                    rowFlags.append(0);
                }
                col += cell.columnSpan;
                maxCol = qMax(maxCol, col);
            }

            table.columnCount = qMax(table.columnCount, maxCol);
            table.rows.append(row);
            vMergeFlags.append(rowFlags);
            table.rowMinHeightsPt.append(rowMinPt);
        }

        if (!gridWidths.isEmpty()) {
            table.columnWeights = gridWidths;
            table.columnCount = qMax(table.columnCount, gridWidths.size());
        }

        // Pad rows to full grid.
        for (int r = 0; r < table.rows.size(); ++r) {
            auto &row = table.rows[r];
            auto &flags = vMergeFlags[r];
            while (flags.size() < row.size())
                flags.append(0);
            while (row.size() < table.columnCount) {
                DocTableCell blank;
                ensureBlankParagraphs(blank);
                row.append(blank);
                flags.append(0);
            }
        }

        // Resolve vMerge restart → rowSpan; mark continue cells covered.
        for (int r = 0; r < table.rows.size(); ++r) {
            for (int c = 0; c < table.columnCount; ++c) {
                if (c >= vMergeFlags[r].size() || vMergeFlags[r][c] != 1)
                    continue;
                DocTableCell &anchor = table.rows[r][c];
                const int colSpan = qMax(1, anchor.columnSpan);
                int rowSpan = 1;
                for (int rr = r + 1; rr < table.rows.size(); ++rr) {
                    if (c >= vMergeFlags[rr].size() || vMergeFlags[rr][c] != 2)
                        break;
                    ++rowSpan;
                    for (int k = 0; k < colSpan && c + k < table.columnCount; ++k) {
                        table.rows[rr][c + k] = makeCovered();
                        table.rows[rr][c + k].columnSpan = 1;
                    }
                }
                anchor.rowSpan = rowSpan;
                anchor.covered = false;
            }
        }

        while (table.rowMinHeightsPt.size() < table.rows.size())
            table.rowMinHeightsPt.append(0);
        return table;
    }

    ZipReader *m_zip = nullptr;
    QHash<QString, RelInfo> m_rels;
};

} // namespace

bool load(DocumentModel *model, const QString &filePath, QString *errorMessage,
          DocxDocumentMeta *meta)
{
    if (!model) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效文档模型。");
        return false;
    }

    ZipReader zip(filePath);
    if (!zip.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法打开 DOCX：文件无效或已损坏。");
        return false;
    }

    model->sections.clear();
    model->footnoteBodies.clear();
    model->footnoteOrder.clear();
    model->endnoteBodies.clear();
    model->endnoteOrder.clear();
    model->comments.clear();
    model->commentOrder.clear();
    Importer importer(&zip);
    if (!importer.read(model, errorMessage))
        return false;

    if (meta)
        (void)DocxPackage::readMeta(filePath, meta);

    if (meta && meta->writePageLayout && !model->sections.isEmpty()) {
        // Prefer package meta page layout when present; else keep sectPr from body.
        if (meta->pageLayout.pageSizeMm().width() > 10)
            model->sections.first().pageSetup = meta->pageLayout;
    }
    return true;
}

} // namespace DocxImporter
} // namespace Engine
