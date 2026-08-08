#include "formulaio.h"
#include "formularenderer.h"

#include <QBuffer>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <memory>
#include <utility>

namespace FormulaIO {
namespace {

struct FormulaHit
{
    int position = 0;
    int length = 0;
    QString latex;
    qreal pointSize = 18.0;
    QImage image;
};

QList<FormulaHit> collectFormulaHits(QTextDocument *document)
{
    QList<FormulaHit> hits;
    if (!document)
        return hits;

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat())
                continue;

            const QTextImageFormat fmt = frag.charFormat().toImageFormat();
            QString latex = latexFromAlt(fmt.stringProperty(QTextFormat::ImageAltText));
            if (latex.isEmpty() && FormulaRenderer::isFormulaResource(fmt.name()))
                latex = FormulaRenderer::latexFromResourceName(fmt.name());
            if (latex.isEmpty()) {
                const int marker = fmt.name().indexOf(QStringLiteral("#nwlatex="));
                if (marker >= 0) {
                    const QByteArray encoded = fmt.name().mid(marker + 9).toLatin1();
                    latex = QString::fromUtf8(
                        QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
                }
            }
            if (latex.isEmpty())
                continue;

            FormulaHit hit{
                .position = frag.position(),
                .length = frag.length(),
                .latex = latex,
            };
            if (fmt.hasProperty(PointSizeProperty))
                hit.pointSize = fmt.doubleProperty(PointSizeProperty);
            else if (fmt.height() > 0)
                hit.pointSize = qBound(12.0, 36.0, fmt.height() * 0.55);

            const QVariant res = document->resource(QTextDocument::ImageResource, QUrl(fmt.name()));
            if (res.canConvert<QImage>())
                hit.image = res.value<QImage>();
            if (hit.image.isNull() && fmt.name().startsWith(QLatin1String("data:image"))) {
                QByteArray dataUrl = fmt.name().toUtf8();
                const int hash = dataUrl.indexOf('#');
                if (hash > 0)
                    dataUrl = dataUrl.left(hash);
                const int comma = dataUrl.indexOf(',');
                if (comma > 0)
                    hit.image.loadFromData(QByteArray::fromBase64(dataUrl.mid(comma + 1)));
            }
            hits.append(std::move(hit));
        }
    }
    return hits;
}

void replaceHitsDescending(QTextDocument *document, QList<FormulaHit> hits, qreal dpr,
                           bool asDataUrl)
{
    if (!document)
        return;

    std::sort(hits.begin(), hits.end(),
              [](const FormulaHit &a, const FormulaHit &b) { return a.position > b.position; });

    for (FormulaHit &hit : hits) {
        if (hit.image.isNull())
            hit.image = FormulaRenderer::render(hit.latex, hit.pointSize, dpr);
        if (hit.image.isNull())
            continue;

        QTextImageFormat fmt;
        fmt.setProperty(PointSizeProperty, hit.pointSize);
        fmt.setProperty(QTextFormat::ImageAltText, makeLatexAlt(hit.latex));

        const qreal imageDpr = qMax<qreal>(1.0, hit.image.devicePixelRatio());
        fmt.setWidth(hit.image.width() / imageDpr);
        fmt.setHeight(hit.image.height() / imageDpr);

        if (asDataUrl) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            QImage toSave = hit.image;
            if (toSave.format() != QImage::Format_ARGB32
                && toSave.format() != QImage::Format_ARGB32_Premultiplied)
                toSave = toSave.convertToFormat(QImage::Format_ARGB32);
            if (!toSave.save(&buffer, "PNG"))
                continue;
            const QByteArray latexEnc =
                FormulaRenderer::stripMathDelimiters(hit.latex)
                    .toUtf8()
                    .toBase64(QByteArray::Base64UrlEncoding);
            fmt.setName(QStringLiteral("data:image/png;base64,")
                        + QString::fromLatin1(bytes.toBase64())
                        + QStringLiteral("#nwlatex=")
                        + QString::fromLatin1(latexEnc));
        } else {
            const QString name = FormulaRenderer::resourceNameForLatex(hit.latex, hit.pointSize);
            document->addResource(QTextDocument::ImageResource, QUrl(name), hit.image);
            fmt.setName(name);
        }

        QTextCursor cursor(document);
        cursor.setPosition(hit.position);
        cursor.setPosition(hit.position + hit.length, QTextCursor::KeepAnchor);
        cursor.insertImage(fmt);
    }
}

} // namespace

QString makeLatexAlt(const QString &latex)
{
    return QStringLiteral("latex:") + FormulaRenderer::stripMathDelimiters(latex);
}

QString latexFromAlt(const QString &alt)
{
    if (!alt.startsWith(QLatin1String("latex:")))
        return {};
    return FormulaRenderer::stripMathDelimiters(alt.mid(6));
}

void embedFormulasForHtml(QTextDocument *document)
{
    replaceHitsDescending(document, collectFormulaHits(document), 2.0, true);
}

void restoreFormulasFromHtml(QTextDocument *document, qreal devicePixelRatio)
{
    replaceHitsDescending(document, collectFormulaHits(document), devicePixelRatio, false);
}

void replaceFormulasWithMarkdownMarkers(QTextDocument *document)
{
    if (!document)
        return;

    QList<FormulaHit> hits = collectFormulaHits(document);
    std::sort(hits.begin(), hits.end(),
              [](const FormulaHit &a, const FormulaHit &b) { return a.position > b.position; });

    for (const FormulaHit &hit : hits) {
        QTextCursor cursor(document);
        cursor.setPosition(hit.position);
        cursor.setPosition(hit.position + hit.length, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("$$")
                          + FormulaRenderer::stripMathDelimiters(hit.latex)
                          + QStringLiteral("$$"));
    }
}

QString extractMarkdownFormulas(const QString &markdown, QStringList *formulas)
{
    if (!formulas)
        return markdown;
    formulas->clear();

    QString out = markdown;

    auto replaceAll = [&](const QRegularExpression &re) {
        QList<QRegularExpressionMatch> matches;
        auto it = re.globalMatch(out);
        while (it.hasNext())
            matches.append(it.next());

        for (int i = matches.size() - 1; i >= 0; --i) {
            const QRegularExpressionMatch &m = matches.at(i);
            const QString body = m.captured(1).trimmed();
            if (body.isEmpty())
                continue;
            const int index = formulas->size();
            formulas->append(FormulaRenderer::stripMathDelimiters(body));
            out.replace(m.capturedStart(), m.capturedLength(),
                        QStringLiteral("@@NWFORMULA_%1@@").arg(index));
        }
    };

    replaceAll(QRegularExpression(QStringLiteral("\\$\\$([\\s\\S]+?)\\$\\$")));
    replaceAll(QRegularExpression(QStringLiteral("\\\\\\[([\\s\\S]+?)\\\\\\]")));
    replaceAll(QRegularExpression(QStringLiteral("\\\\\\(([\\s\\S]+?)\\\\\\)")));
    replaceAll(QRegularExpression(QStringLiteral("(?<!\\$)\\$([^\\$\\n]+?)\\$(?!\\$)")));

    return out;
}

void injectMarkdownFormulas(QTextDocument *document, const QStringList &formulas,
                            qreal devicePixelRatio)
{
    if (!document || formulas.isEmpty())
        return;

    for (int i = formulas.size() - 1; i >= 0; --i) {
        const QString token = QStringLiteral("@@NWFORMULA_%1@@").arg(i);
        while (true) {
            QTextCursor cursor = document->find(token);
            if (cursor.isNull())
                break;
            const QString latex = formulas.at(i);
            constexpr qreal pointSize = 18.0;
            QImage image = FormulaRenderer::render(latex, pointSize, devicePixelRatio);
            if (image.isNull()) {
                cursor.insertText(QStringLiteral("$$") + latex + QStringLiteral("$$"));
                continue;
            }
            const QString name = FormulaRenderer::resourceNameForLatex(latex, pointSize);
            document->addResource(QTextDocument::ImageResource, QUrl(name), image);
            QTextImageFormat fmt;
            fmt.setName(name);
            fmt.setWidth(image.width() / qMax<qreal>(1.0, devicePixelRatio));
            fmt.setHeight(image.height() / qMax<qreal>(1.0, devicePixelRatio));
            fmt.setProperty(PointSizeProperty, pointSize);
            fmt.setProperty(QTextFormat::ImageAltText, makeLatexAlt(latex));
            cursor.insertImage(fmt);
        }
    }
}

QString documentToHtmlWithFormulas(QTextDocument *source)
{
    if (!source)
        return {};
    std::unique_ptr<QTextDocument> clone(source->clone());
    embedFormulasForHtml(clone.get());
    return clone->toHtml();
}

QString documentToMarkdownWithFormulas(QTextDocument *source)
{
    if (!source)
        return {};
    std::unique_ptr<QTextDocument> clone(source->clone());
    replaceFormulasWithMarkdownMarkers(clone.get());
    return clone->toMarkdown(QTextDocument::MarkdownDialectGitHub);
}

} // namespace FormulaIO
