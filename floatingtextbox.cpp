#include "floatingtextbox.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextDocument>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace FloatingTextBoxes {
namespace {

QVariantMap toMap(const FloatingTextBox &box)
{
    QVariantMap m;
    m.insert(QStringLiteral("id"), box.id);
    m.insert(QStringLiteral("pageIndex"), box.pageIndex);
    m.insert(QStringLiteral("xPt"), box.xPt);
    m.insert(QStringLiteral("yPt"), box.yPt);
    m.insert(QStringLiteral("wPt"), box.wPt);
    m.insert(QStringLiteral("hPt"), box.hPt);
    m.insert(QStringLiteral("html"), box.html);
    return m;
}

FloatingTextBox fromMap(const QVariantMap &m)
{
    FloatingTextBox box;
    box.id = m.value(QStringLiteral("id")).toString();
    box.pageIndex = qMax(0, m.value(QStringLiteral("pageIndex")).toInt());
    box.xPt = m.value(QStringLiteral("xPt"), 72).toReal();
    box.yPt = m.value(QStringLiteral("yPt"), 72).toReal();
    box.wPt = qMax(40.0, m.value(QStringLiteral("wPt"), 180).toReal());
    box.hPt = qMax(30.0, m.value(QStringLiteral("hPt"), 90).toReal());
    box.html = m.value(QStringLiteral("html")).toString();
    if (box.id.isEmpty())
        box.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return box;
}

QJsonObject toJsonObject(const FloatingTextBox &box)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), box.id);
    o.insert(QStringLiteral("pageIndex"), box.pageIndex);
    o.insert(QStringLiteral("xPt"), box.xPt);
    o.insert(QStringLiteral("yPt"), box.yPt);
    o.insert(QStringLiteral("wPt"), box.wPt);
    o.insert(QStringLiteral("hPt"), box.hPt);
    o.insert(QStringLiteral("html"), box.html);
    return o;
}

FloatingTextBox fromJsonObject(const QJsonObject &o)
{
    QVariantMap m;
    for (auto it = o.begin(); it != o.end(); ++it)
        m.insert(it.key(), it.value().toVariant());
    return fromMap(m);
}

} // namespace

QVector<FloatingTextBox> load(const QTextDocument *document)
{
    QVector<FloatingTextBox> out;
    if (!document)
        return out;
    const QVariant res =
        document->resource(QTextDocument::UserResource, QUrl(QLatin1String(kResourceUrl)));
    const QVariantList list = res.toList();
    out.reserve(list.size());
    for (const QVariant &v : list)
        out.append(fromMap(v.toMap()));
    return out;
}

void save(QTextDocument *document, const QVector<FloatingTextBox> &boxes, bool markModified)
{
    if (!document)
        return;
    QVariantList list;
    list.reserve(boxes.size());
    for (const FloatingTextBox &box : boxes)
        list.append(toMap(box));
    document->addResource(QTextDocument::UserResource, QUrl(QLatin1String(kResourceUrl)), list);
    if (markModified)
        document->setModified(true);
}

FloatingTextBox makeDefault(int pageIndex)
{
    FloatingTextBox box;
    box.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    box.pageIndex = qMax(0, pageIndex);
    box.xPt = 72;
    box.yPt = 72;
    box.wPt = 180;
    box.hPt = 90;
    box.html = QStringLiteral("<p>在此输入文字</p>");
    return box;
}

QString toJson(const QVector<FloatingTextBox> &boxes)
{
    QJsonArray arr;
    for (const FloatingTextBox &box : boxes)
        arr.append(toJsonObject(box));
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<FloatingTextBox> fromJson(const QString &json)
{
    QVector<FloatingTextBox> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray())
        return out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (v.isObject())
            out.append(fromJsonObject(v.toObject()));
    }
    return out;
}

QString embedInHtml(const QString &html, const QVector<FloatingTextBox> &boxes)
{
    QString body = stripMarkerFromHtml(html);
    if (boxes.isEmpty())
        return body;
    const QByteArray b64 = toJson(boxes).toUtf8().toBase64();
    body += QLatin1Char('\n');
    body += QLatin1String(kHtmlMarkerPrefix);
    body += QString::fromLatin1(b64);
    body += QLatin1String(kHtmlMarkerSuffix);
    body += QLatin1Char('\n');
    return body;
}

QVector<FloatingTextBox> extractFromHtml(const QString &html)
{
    const int start = html.indexOf(QLatin1String(kHtmlMarkerPrefix));
    if (start < 0)
        return {};
    const int dataStart = start + int(sizeof(kHtmlMarkerPrefix) - 1);
    const int end = html.indexOf(QLatin1String(kHtmlMarkerSuffix), dataStart);
    if (end < 0)
        return {};
    const QByteArray b64 = html.mid(dataStart, end - dataStart).toLatin1();
    return fromJson(QString::fromUtf8(QByteArray::fromBase64(b64)));
}

QString stripMarkerFromHtml(const QString &html)
{
    const int start = html.indexOf(QLatin1String(kHtmlMarkerPrefix));
    if (start < 0)
        return html;
    const int end = html.indexOf(QLatin1String(kHtmlMarkerSuffix), start);
    if (end < 0)
        return html;
    QString out = html;
    out.remove(start, end + int(sizeof(kHtmlMarkerSuffix) - 1) - start);
    return out.trimmed();
}

QByteArray toXmlBytes(const QVector<FloatingTextBox> &boxes)
{
    QByteArray bytes;
    QXmlStreamWriter w(&bytes);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("newwordFloatingBoxes"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
    for (const FloatingTextBox &box : boxes) {
        w.writeStartElement(QStringLiteral("box"));
        w.writeAttribute(QStringLiteral("id"), box.id);
        w.writeAttribute(QStringLiteral("pageIndex"), QString::number(box.pageIndex));
        w.writeAttribute(QStringLiteral("xPt"), QString::number(box.xPt, 'f', 2));
        w.writeAttribute(QStringLiteral("yPt"), QString::number(box.yPt, 'f', 2));
        w.writeAttribute(QStringLiteral("wPt"), QString::number(box.wPt, 'f', 2));
        w.writeAttribute(QStringLiteral("hPt"), QString::number(box.hPt, 'f', 2));
        w.writeStartElement(QStringLiteral("html"));
        w.writeCDATA(box.html);
        w.writeEndElement();
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return bytes;
}

QVector<FloatingTextBox> fromXmlBytes(const QByteArray &bytes)
{
    QVector<FloatingTextBox> out;
    QXmlStreamReader r(bytes);
    FloatingTextBox current;
    bool inBox = false;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QLatin1String("box")) {
            current = FloatingTextBox{};
            current.id = r.attributes().value(QLatin1String("id")).toString();
            current.pageIndex = r.attributes().value(QLatin1String("pageIndex")).toInt();
            current.xPt = r.attributes().value(QLatin1String("xPt")).toDouble();
            current.yPt = r.attributes().value(QLatin1String("yPt")).toDouble();
            current.wPt = r.attributes().value(QLatin1String("wPt")).toDouble();
            current.hPt = r.attributes().value(QLatin1String("hPt")).toDouble();
            inBox = true;
        } else if (inBox && r.isStartElement() && r.name() == QLatin1String("html")) {
            current.html = r.readElementText();
        } else if (inBox && r.isEndElement() && r.name() == QLatin1String("box")) {
            if (current.id.isEmpty())
                current.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            current.wPt = qMax(40.0, current.wPt);
            current.hPt = qMax(30.0, current.hPt);
            out.append(current);
            inBox = false;
        }
    }
    return out;
}

} // namespace FloatingTextBoxes
