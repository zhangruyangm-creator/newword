#ifndef FORMULAIO_H
#define FORMULAIO_H

#include <QString>
#include <QTextFormat>

class QTextDocument;

namespace FormulaIO {

constexpr int PointSizeProperty = QTextFormat::UserProperty + 77;

QString makeLatexAlt(const QString &latex);
QString latexFromAlt(const QString &alt);

void embedFormulasForHtml(QTextDocument *document);
void restoreFormulasFromHtml(QTextDocument *document, qreal devicePixelRatio = 2.0);

void replaceFormulasWithMarkdownMarkers(QTextDocument *document);
QString extractMarkdownFormulas(const QString &markdown, QStringList *formulas);
void injectMarkdownFormulas(QTextDocument *document, const QStringList &formulas,
                            qreal devicePixelRatio = 2.0);

QString documentToHtmlWithFormulas(QTextDocument *source);
QString documentToMarkdownWithFormulas(QTextDocument *source);

} // namespace FormulaIO

#endif // FORMULAIO_H
