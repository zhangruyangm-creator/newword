#include "docxpackage.h"
#include "docxcommon.h"
#include "floatingtextbox.h"
#include "ziparchive.h"

#include <QBuffer>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace DocxPackage {
namespace {

int mmToTwips(qreal mm)
{
    return qRound(mm * 1440.0 / 25.4);
}

QByteArray plainHeaderXml(const QString &text)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("w:hdr"));
    w.writeAttribute(QStringLiteral("xmlns:w"),
                     QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));
    w.writeStartElement(QStringLiteral("w:p"));
    w.writeStartElement(QStringLiteral("w:r"));
    w.writeStartElement(QStringLiteral("w:t"));
    w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
    w.writeCharacters(text);
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return xml;
}

QByteArray plainFooterXml(const QString &text, bool showPageNumber)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("w:ftr"));
    w.writeAttribute(QStringLiteral("xmlns:w"),
                     QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"));
    w.writeStartElement(QStringLiteral("w:p"));
    if (!text.isEmpty()) {
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:t"));
        w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
        w.writeCharacters(text);
        w.writeEndElement();
        w.writeEndElement();
    }
    if (showPageNumber) {
        if (!text.isEmpty()) {
            w.writeStartElement(QStringLiteral("w:r"));
            w.writeStartElement(QStringLiteral("w:t"));
            w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
            w.writeCharacters(QStringLiteral("  "));
            w.writeEndElement();
            w.writeEndElement();
        }
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:fldChar"));
        w.writeAttribute(QStringLiteral("w:fldCharType"), QStringLiteral("begin"));
        w.writeEndElement(); // fldChar
        w.writeEndElement(); // r
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:instrText"));
        w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
        w.writeCharacters(QStringLiteral(" PAGE "));
        w.writeEndElement();
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:fldChar"));
        w.writeAttribute(QStringLiteral("w:fldCharType"), QStringLiteral("end"));
        w.writeEndElement(); // fldChar
        w.writeEndElement(); // r
    }
    w.writeEndElement(); // p
    w.writeEndElement(); // ftr
    w.writeEndDocument();
    return xml;
}

QString extractPlainTextFromPart(const QByteArray &xml)
{
    if (xml.isEmpty())
        return {};
    QString out;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        if (reader.readNext() == QXmlStreamReader::StartElement
            && reader.name() == QLatin1String("t")) {
            out += reader.readElementText();
        }
    }
    return out.trimmed();
}

QByteArray patchSectPr(QByteArray documentXml, const DocxDocumentMeta &meta,
                       bool hasHeader, bool hasFooter)
{
    const PageLayoutSettings &layout = meta.pageLayout;
    const QSizeF mm = layout.pageSizeMm();
    const int w = mmToTwips(mm.width());
    const int h = mmToTwips(mm.height());
    const int top = mmToTwips(layout.marginsMm.top());
    const int bottom = mmToTwips(layout.marginsMm.bottom());
    const int left = mmToTwips(layout.marginsMm.left());
    const int right = mmToTwips(layout.marginsMm.right());
    const int header = mmToTwips(layout.headerDistanceMm);
    const int footer = mmToTwips(layout.footerDistanceMm);

    QString refs;
    if (hasHeader)
        refs += QStringLiteral("<w:headerReference w:type=\"default\" r:id=\"rIdHdr\"/>");
    if (hasFooter)
        refs += QStringLiteral("<w:footerReference w:type=\"default\" r:id=\"rIdFtr\"/>");

    const QString sect = QStringLiteral(
        "<w:sectPr>"
        "%1"
        "<w:pgSz w:w=\"%2\" w:h=\"%3\"/>"
        "<w:pgMar w:top=\"%4\" w:right=\"%5\" w:bottom=\"%6\" w:left=\"%7\" "
        "w:header=\"%8\" w:footer=\"%9\"/>"
        "</w:sectPr>")
                             .arg(refs)
                             .arg(w)
                             .arg(h)
                             .arg(top)
                             .arg(right)
                             .arg(bottom)
                             .arg(left)
                             .arg(header)
                             .arg(footer);

    QString doc = QString::fromUtf8(documentXml);
    static const QRegularExpression re(
        QStringLiteral("<w:sectPr\\b[\\s\\S]*?</w:sectPr>"));
    if (re.match(doc).hasMatch())
        doc.replace(re, sect);
    else
        doc.replace(QStringLiteral("</w:body>"), sect + QStringLiteral("</w:body>"));
    return doc.toUtf8();
}

void ensureRelationship(QByteArray *relsXml, const QString &id, const QString &type,
                        const QString &target)
{
    QString rels = QString::fromUtf8(*relsXml);
    if (rels.contains(QStringLiteral("Id=\"%1\"").arg(id)))
        return;
    const QString node = QStringLiteral(
        "<Relationship Id=\"%1\" Type=\"%2\" Target=\"%3\"/>").arg(id, type, target);
    rels.replace(QStringLiteral("</Relationships>"), node + QStringLiteral("</Relationships>"));
    *relsXml = rels.toUtf8();
}

void ensureContentType(QByteArray *ct, const QString &partName, const QString &contentType)
{
    QString s = QString::fromUtf8(*ct);
    if (s.contains(partName))
        return;
    const QString node = QStringLiteral(
        "<Override PartName=\"%1\" ContentType=\"%2\"/>").arg(partName, contentType);
    s.replace(QStringLiteral("</Types>"), node + QStringLiteral("</Types>"));
    *ct = s.toUtf8();
}

} // namespace

bool readMeta(const QString &filePath, DocxDocumentMeta *meta)
{
    if (!meta)
        return false;
    ZipReader zip(filePath);
    if (!zip.isValid())
        return false;

    const QByteArray hdr = zip.fileData(QStringLiteral("word/header1.xml"));
    if (hdr.isEmpty()) {
        // Try any header*.xml
        for (const QString &name : zip.fileNames()) {
            if (name.startsWith(QStringLiteral("word/header")) && name.endsWith(QStringLiteral(".xml"))) {
                meta->headerFooter.header = extractPlainTextFromPart(zip.fileData(name));
                break;
            }
        }
    } else {
        meta->headerFooter.header = extractPlainTextFromPart(hdr);
    }

    const QByteArray ftr = zip.fileData(QStringLiteral("word/footer1.xml"));
    if (!ftr.isEmpty()) {
        QString t = extractPlainTextFromPart(ftr);
        // Strip trailing page-number leftovers if any.
        meta->headerFooter.footer = t;
        meta->headerFooter.showPageNumber = ftr.contains("PAGE");
    } else {
        for (const QString &name : zip.fileNames()) {
            if (name.startsWith(QStringLiteral("word/footer")) && name.endsWith(QStringLiteral(".xml"))) {
                const QByteArray data = zip.fileData(name);
                meta->headerFooter.footer = extractPlainTextFromPart(data);
                meta->headerFooter.showPageNumber = data.contains("PAGE");
                break;
            }
        }
    }

    const QByteArray documentXml = zip.fileData(QStringLiteral("word/document.xml"));
    if (!documentXml.isEmpty()) {
        QXmlStreamReader reader(documentXml);
        while (!reader.atEnd()) {
            if (reader.readNext() != QXmlStreamReader::StartElement)
                continue;
            if (reader.name() == QLatin1String("pgSz")) {
                const qreal wTw = reader.attributes().value(QStringLiteral("w:w")).toDouble();
                const qreal hTw = reader.attributes().value(QStringLiteral("w:h")).toDouble();
                if (wTw > 0 && hTw > 0) {
                    meta->pageLayout.paper = PageLayoutSettings::Paper::Custom;
                    meta->pageLayout.customWidthMm = wTw * 25.4 / 1440.0;
                    meta->pageLayout.customHeightMm = hTw * 25.4 / 1440.0;
                }
            } else if (reader.name() == QLatin1String("pgMar")) {
                auto mm = [&](const QString &a) {
                    return reader.attributes().value(a).toDouble() * 25.4 / 1440.0;
                };
                meta->pageLayout.marginsMm = QMarginsF(mm(QStringLiteral("w:left")),
                                                       mm(QStringLiteral("w:top")),
                                                       mm(QStringLiteral("w:right")),
                                                       mm(QStringLiteral("w:bottom")));
                if (reader.attributes().hasAttribute(QStringLiteral("w:header")))
                    meta->pageLayout.headerDistanceMm = mm(QStringLiteral("w:header"));
                if (reader.attributes().hasAttribute(QStringLiteral("w:footer")))
                    meta->pageLayout.footerDistanceMm = mm(QStringLiteral("w:footer"));
            }
        }
    }
    return true;
}

bool applyMeta(const QString &filePath, const DocxDocumentMeta &meta, QString *errorMessage)
{
    ZipReader reader(filePath);
    if (!reader.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取 DOCX 以写入页眉页脚。");
        return false;
    }

    const bool hasHeader = meta.writeHeaderFooter
        && (!meta.headerFooter.header.isEmpty() || meta.headerFooter.differentFirstPage
            || meta.headerFooter.differentOddEven);
    const bool hasFooter = meta.writeHeaderFooter
        && (!meta.headerFooter.footer.isEmpty() || meta.headerFooter.showPageNumber
            || meta.headerFooter.differentFirstPage || meta.headerFooter.differentOddEven);
    // Always write HF parts if writeHeaderFooter — even empty header with page-number footer.
    const bool writeHf = meta.writeHeaderFooter;

    QHash<QString, QByteArray> files;
    for (const QString &name : reader.fileNames())
        files.insert(name, reader.fileData(name));

    if (writeHf) {
        files.insert(QStringLiteral("word/header1.xml"),
                     plainHeaderXml(meta.headerFooter.header));
        files.insert(QStringLiteral("word/footer1.xml"),
                     plainFooterXml(meta.headerFooter.footer, meta.headerFooter.showPageNumber));
    }

    if (meta.writePageLayout || writeHf) {
        QByteArray doc = files.value(QStringLiteral("word/document.xml"));
        if (!doc.isEmpty()) {
            // Ensure r namespace on document root if adding header refs
            if (writeHf && !doc.contains(QByteArrayLiteral("xmlns:r="))) {
                doc.replace(QByteArrayLiteral("<w:document"),
                            QByteArrayLiteral(
                                "<w:document xmlns:r=\"http://schemas.openxmlformats.org/"
                                "officeDocument/2006/relationships\""));
            }
            files.insert(QStringLiteral("word/document.xml"),
                         patchSectPr(doc, meta, writeHf, writeHf));
        }
    }

    QByteArray rels = files.value(QStringLiteral("word/_rels/document.xml.rels"));
    if (rels.isEmpty()) {
        rels = QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "</Relationships>");
    }
    if (writeHf) {
        ensureRelationship(
            &rels, QStringLiteral("rIdHdr"),
            QStringLiteral(
                "http://schemas.openxmlformats.org/officeDocument/2006/relationships/header"),
            QStringLiteral("header1.xml"));
        ensureRelationship(
            &rels, QStringLiteral("rIdFtr"),
            QStringLiteral(
                "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer"),
            QStringLiteral("footer1.xml"));
    }
    files.insert(QStringLiteral("word/_rels/document.xml.rels"), rels);

    QByteArray ct = files.value(QStringLiteral("[Content_Types].xml"));
    if (!ct.isEmpty() && writeHf) {
        ensureContentType(
            &ct, QStringLiteral("/word/header1.xml"),
            QStringLiteral(
                "application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"));
        ensureContentType(
            &ct, QStringLiteral("/word/footer1.xml"),
            QStringLiteral(
                "application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml"));
        files.insert(QStringLiteral("[Content_Types].xml"), ct);
    }

    ZipWriter writer(filePath);
    if (!writer.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写回 DOCX。");
        return false;
    }
    for (auto it = files.begin(); it != files.end(); ++it)
        writer.addFile(it.key(), it.value());
    if (!writer.close()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写回 DOCX 失败。");
        return false;
    }
    Q_UNUSED(hasHeader);
    Q_UNUSED(hasFooter);
    return true;
}

bool hasFootnotes(const QString &filePath)
{
    ZipReader zip(filePath);
    if (!zip.isValid())
        return false;

    const QByteArray xml = zip.fileData(QStringLiteral("word/footnotes.xml"));
    if (xml.isEmpty())
        return false;

    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("footnote"))
            continue;

        QString type = reader.attributes().value(QLatin1String("type")).toString();
        if (type.isEmpty())
            type = reader.attributes().value(QStringLiteral("w:type")).toString();
        if (type == QLatin1String("separator")
            || type == QLatin1String("continuationSeparator"))
            continue;

        QString id = reader.attributes().value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = reader.attributes().value(QStringLiteral("w:id")).toString();
        bool ok = false;
        const int numericId = id.toInt(&ok);
        if (ok && numericId > 0)
            return true;
    }
    return false;
}

bool hasEndnotes(const QString &filePath)
{
    ZipReader zip(filePath);
    if (!zip.isValid())
        return false;
    const QByteArray xml = zip.fileData(QStringLiteral("word/endnotes.xml"));
    if (xml.isEmpty())
        return false;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("endnote"))
            continue;
        QString type = reader.attributes().value(QLatin1String("type")).toString();
        if (type.isEmpty())
            type = reader.attributes().value(QStringLiteral("w:type")).toString();
        if (type == QLatin1String("separator")
            || type == QLatin1String("continuationSeparator"))
            continue;
        QString id = reader.attributes().value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = reader.attributes().value(QStringLiteral("w:id")).toString();
        bool ok = false;
        const int numericId = id.toInt(&ok);
        if (ok && numericId > 0)
            return true;
    }
    return false;
}

bool hasComments(const QString &filePath)
{
    ZipReader zip(filePath);
    if (!zip.isValid())
        return false;
    const QByteArray xml = zip.fileData(QStringLiteral("word/comments.xml"));
    if (xml.isEmpty())
        return false;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("comment"))
            return true;
    }
    return false;
}

QVector<FloatingTextBox> readFloatingBoxes(const QString &filePath)
{
    ZipReader zip(filePath);
    if (!zip.isValid())
        return {};
    const QByteArray xml = zip.fileData(QLatin1String(FloatingTextBoxes::kDocxPartPath));
    if (xml.isEmpty())
        return {};
    return FloatingTextBoxes::fromXmlBytes(xml);
}

bool writeFloatingBoxes(const QString &filePath, const QVector<FloatingTextBox> &boxes,
                        QString *errorMessage)
{
    ZipReader reader(filePath);
    if (!reader.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取 DOCX 以写入浮动文本框。");
        return false;
    }

    QHash<QString, QByteArray> files;
    for (const QString &name : reader.fileNames())
        files.insert(name, reader.fileData(name));

    const QString part = QLatin1String(FloatingTextBoxes::kDocxPartPath);
    if (boxes.isEmpty()) {
        files.remove(part);
    } else {
        files.insert(part, FloatingTextBoxes::toXmlBytes(boxes));
        QByteArray ct = files.value(QStringLiteral("[Content_Types].xml"));
        if (!ct.isEmpty()) {
            ensureContentType(&ct, QStringLiteral("/") + part,
                              QStringLiteral("application/xml"));
            files.insert(QStringLiteral("[Content_Types].xml"), ct);
        }
    }

    ZipWriter writer(filePath);
    if (!writer.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写回 DOCX（浮动文本框）。");
        return false;
    }
    for (auto it = files.begin(); it != files.end(); ++it)
        writer.addFile(it.key(), it.value());
    if (!writer.close()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写回 DOCX 失败（浮动文本框）。");
        return false;
    }
    return true;
}

} // namespace DocxPackage
