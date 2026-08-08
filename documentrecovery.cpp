#include "documentrecovery.h"
#include "floatingtextbox.h"
#include "formulaio.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTextDocument>

namespace {

QString manifestPath()
{
    return DocumentRecovery::draftsDirectory() + QStringLiteral("/manifest.json");
}

QJsonObject headerFooterToJson(const HeaderFooterSettings &hf)
{
    QJsonObject o;
    o.insert(QStringLiteral("header"), hf.header);
    o.insert(QStringLiteral("footer"), hf.footer);
    o.insert(QStringLiteral("showPageNumber"), hf.showPageNumber);
    o.insert(QStringLiteral("pageNumberFormat"), static_cast<int>(hf.pageNumberFormat));
    o.insert(QStringLiteral("differentFirstPage"), hf.differentFirstPage);
    o.insert(QStringLiteral("firstHeader"), hf.firstHeader);
    o.insert(QStringLiteral("firstFooter"), hf.firstFooter);
    o.insert(QStringLiteral("differentOddEven"), hf.differentOddEven);
    o.insert(QStringLiteral("evenHeader"), hf.evenHeader);
    o.insert(QStringLiteral("evenFooter"), hf.evenFooter);
    return o;
}

HeaderFooterSettings headerFooterFromJson(const QJsonObject &o)
{
    HeaderFooterSettings hf;
    hf.header = o.value(QStringLiteral("header")).toString();
    hf.footer = o.value(QStringLiteral("footer")).toString();
    hf.showPageNumber = o.value(QStringLiteral("showPageNumber")).toBool(true);
    hf.pageNumberFormat = static_cast<HeaderFooterSettings::PageNumberFormat>(
        o.value(QStringLiteral("pageNumberFormat"))
            .toInt(static_cast<int>(HeaderFooterSettings::PageNumberFormat::ChinesePageOf)));
    hf.differentFirstPage = o.value(QStringLiteral("differentFirstPage")).toBool(false);
    hf.firstHeader = o.value(QStringLiteral("firstHeader")).toString();
    hf.firstFooter = o.value(QStringLiteral("firstFooter")).toString();
    hf.differentOddEven = o.value(QStringLiteral("differentOddEven")).toBool(false);
    hf.evenHeader = o.value(QStringLiteral("evenHeader")).toString();
    hf.evenFooter = o.value(QStringLiteral("evenFooter")).toString();
    return hf;
}

QJsonObject pageLayoutToJson(const PageLayoutSettings &layout)
{
    QJsonObject o;
    o.insert(QStringLiteral("paper"), static_cast<int>(layout.paper));
    o.insert(QStringLiteral("orientation"), static_cast<int>(layout.orientation));
    o.insert(QStringLiteral("customWidthMm"), layout.customWidthMm);
    o.insert(QStringLiteral("customHeightMm"), layout.customHeightMm);
    o.insert(QStringLiteral("marginLeft"), layout.marginsMm.left());
    o.insert(QStringLiteral("marginTop"), layout.marginsMm.top());
    o.insert(QStringLiteral("marginRight"), layout.marginsMm.right());
    o.insert(QStringLiteral("marginBottom"), layout.marginsMm.bottom());
    o.insert(QStringLiteral("columnCount"), layout.columnCount);
    o.insert(QStringLiteral("columnSpacingMm"), layout.columnSpacingMm);
    o.insert(QStringLiteral("showPageBorder"), layout.showPageBorder);
    o.insert(QStringLiteral("headerDistanceMm"), layout.headerDistanceMm);
    o.insert(QStringLiteral("footerDistanceMm"), layout.footerDistanceMm);
    return o;
}

PageLayoutSettings pageLayoutFromJson(const QJsonObject &o)
{
    PageLayoutSettings layout;
    layout.paper = static_cast<PageLayoutSettings::Paper>(
        o.value(QStringLiteral("paper")).toInt(static_cast<int>(PageLayoutSettings::Paper::A4)));
    layout.orientation = static_cast<PageLayoutSettings::Orientation>(
        o.value(QStringLiteral("orientation"))
            .toInt(static_cast<int>(PageLayoutSettings::Orientation::Portrait)));
    layout.customWidthMm = o.value(QStringLiteral("customWidthMm")).toDouble(210.0);
    layout.customHeightMm = o.value(QStringLiteral("customHeightMm")).toDouble(297.0);
    layout.marginsMm = QMarginsF(o.value(QStringLiteral("marginLeft")).toDouble(25.4),
                                 o.value(QStringLiteral("marginTop")).toDouble(25.4),
                                 o.value(QStringLiteral("marginRight")).toDouble(25.4),
                                 o.value(QStringLiteral("marginBottom")).toDouble(25.4));
    layout.columnCount = o.value(QStringLiteral("columnCount")).toInt(1);
    layout.columnSpacingMm = o.value(QStringLiteral("columnSpacingMm")).toDouble(10.0);
    layout.showPageBorder = o.value(QStringLiteral("showPageBorder")).toBool(false);
    layout.headerDistanceMm = o.value(QStringLiteral("headerDistanceMm")).toDouble(12.0);
    layout.footerDistanceMm = o.value(QStringLiteral("footerDistanceMm")).toDouble(12.0);
    return layout;
}

QJsonArray readManifestArray()
{
    QFile file(manifestPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isArray() ? doc.array() : QJsonArray{};
}

bool writeManifestArray(const QJsonArray &arr)
{
    QDir().mkpath(DocumentRecovery::draftsDirectory());
    QFile file(manifestPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

namespace DocumentRecovery {

QString draftsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/drafts");
}

QVector<Draft> listDrafts()
{
    QVector<Draft> out;
    const QJsonArray arr = readManifestArray();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Draft d;
        d.id = o.value(QStringLiteral("id")).toString();
        d.sourcePath = o.value(QStringLiteral("sourcePath")).toString();
        d.displayName = o.value(QStringLiteral("displayName")).toString();
        d.savedAt = QDateTime::fromString(o.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
        d.headerFooter = headerFooterFromJson(o.value(QStringLiteral("headerFooter")).toObject());
        d.pageLayout = pageLayoutFromJson(o.value(QStringLiteral("pageLayout")).toObject());
        d.htmlFilePath = draftsDirectory() + QLatin1Char('/') + d.id + QStringLiteral(".html");
        if (d.id.isEmpty() || !QFile::exists(d.htmlFilePath))
            continue;
        out.append(d);
    }
    return out;
}

bool writeDraft(const QString &id,
                const QString &sourcePath,
                const QString &displayName,
                QTextDocument *document,
                const HeaderFooterSettings &headerFooter,
                const PageLayoutSettings &pageLayout,
                QString *errorMessage)
{
    if (id.isEmpty() || !document) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效草稿。");
        return false;
    }

    QDir().mkpath(draftsDirectory());
    const QString htmlPath = draftsDirectory() + QLatin1Char('/') + id + QStringLiteral(".html");
    QFile htmlFile(htmlPath);
    if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = htmlFile.errorString();
        return false;
    }
    const QString html = FloatingTextBoxes::embedInHtml(
        FormulaIO::documentToHtmlWithFormulas(document), FloatingTextBoxes::load(document));
    const QByteArray htmlBytes = html.toUtf8();
    if (htmlFile.write(htmlBytes) != htmlBytes.size()) {
        if (errorMessage)
            *errorMessage = htmlFile.errorString().isEmpty()
                ? QStringLiteral("写入草稿失败（可能磁盘已满）")
                : htmlFile.errorString();
        return false;
    }
    htmlFile.close();

    QJsonArray arr = readManifestArray();
    QJsonObject entry;
    entry.insert(QStringLiteral("id"), id);
    entry.insert(QStringLiteral("sourcePath"), sourcePath);
    entry.insert(QStringLiteral("displayName"), displayName);
    entry.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    entry.insert(QStringLiteral("headerFooter"), headerFooterToJson(headerFooter));
    entry.insert(QStringLiteral("pageLayout"), pageLayoutToJson(pageLayout));

    bool replaced = false;
    for (int i = 0; i < arr.size(); ++i) {
        if (arr.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
            arr.replace(i, entry);
            replaced = true;
            break;
        }
    }
    if (!replaced)
        arr.append(entry);

    if (!writeManifestArray(arr)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写入草稿清单。");
        return false;
    }
    return true;
}

bool loadDraftHtml(const Draft &draft, QString *html, QString *errorMessage)
{
    if (!html)
        return false;
    QFile file(draft.htmlFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    *html = QString::fromUtf8(file.readAll());
    return true;
}

bool removeDraft(const QString &id)
{
    if (id.isEmpty())
        return false;
    QFile::remove(draftsDirectory() + QLatin1Char('/') + id + QStringLiteral(".html"));
    QJsonArray arr = readManifestArray();
    QJsonArray next;
    for (const QJsonValue &v : arr) {
        if (v.toObject().value(QStringLiteral("id")).toString() != id)
            next.append(v);
    }
    return writeManifestArray(next);
}

void removeAllDrafts()
{
    const QVector<Draft> drafts = listDrafts();
    for (const Draft &d : drafts)
        removeDraft(d.id);
    QFile::remove(manifestPath());
}

} // namespace DocumentRecovery

namespace EditorDefaults {
namespace {

QFont g_font(QStringLiteral("PingFang SC"), 12);
PageLayoutSettings g_layout;
HeaderFooterSettings g_headerFooter;
bool g_loaded = false;

} // namespace

void loadFromSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("editorDefaults"));
    const QString family = settings.value(QStringLiteral("fontFamily"),
                                          QStringLiteral("PingFang SC")).toString();
    const int pointSize = settings.value(QStringLiteral("fontPointSize"), 12).toInt();
    g_font = QFont(family, qBound(8, pointSize, 72));
    g_layout.paper = static_cast<PageLayoutSettings::Paper>(
        settings.value(QStringLiteral("paper"),
                       static_cast<int>(PageLayoutSettings::Paper::A4)).toInt());
    g_layout.orientation = static_cast<PageLayoutSettings::Orientation>(
        settings.value(QStringLiteral("orientation"),
                       static_cast<int>(PageLayoutSettings::Orientation::Portrait)).toInt());
    g_headerFooter.showPageNumber =
        settings.value(QStringLiteral("showPageNumber"), true).toBool();
    g_headerFooter.pageNumberFormat = static_cast<HeaderFooterSettings::PageNumberFormat>(
        settings.value(QStringLiteral("pageNumberFormat"),
                       static_cast<int>(HeaderFooterSettings::PageNumberFormat::ChinesePageOf))
            .toInt());
    settings.endGroup();
    g_loaded = true;
}

void saveToSettings(const QFont &font, const PageLayoutSettings &layout,
                    const HeaderFooterSettings &headerFooter)
{
    g_font = font;
    g_layout = layout;
    g_headerFooter = headerFooter;
    g_loaded = true;

    QSettings settings;
    settings.beginGroup(QStringLiteral("editorDefaults"));
    settings.setValue(QStringLiteral("fontFamily"), font.family());
    settings.setValue(QStringLiteral("fontPointSize"), font.pointSize());
    settings.setValue(QStringLiteral("paper"), static_cast<int>(layout.paper));
    settings.setValue(QStringLiteral("orientation"), static_cast<int>(layout.orientation));
    settings.setValue(QStringLiteral("showPageNumber"), headerFooter.showPageNumber);
    settings.setValue(QStringLiteral("pageNumberFormat"),
                      static_cast<int>(headerFooter.pageNumberFormat));
    settings.endGroup();
}

QFont documentFont()
{
    if (!g_loaded)
        loadFromSettings();
    return g_font;
}

PageLayoutSettings pageLayout()
{
    if (!g_loaded)
        loadFromSettings();
    return g_layout;
}

HeaderFooterSettings headerFooter()
{
    if (!g_loaded)
        loadFromSettings();
    return g_headerFooter;
}

} // namespace EditorDefaults
