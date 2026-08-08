#ifndef FORMULARENDERER_H
#define FORMULARENDERER_H

#include <QImage>
#include <QString>

class FormulaRenderer
{
public:
    static QString stripMathDelimiters(const QString &latex);
    [[nodiscard]] static QImage render(const QString &latex, qreal pointSize = 18.0, qreal dpr = 2.0);

    [[nodiscard]] static QString resourceNameForLatex(const QString &latex, qreal pointSize = 18.0);
    [[nodiscard]] static bool isFormulaResource(const QString &name);
    [[nodiscard]] static QString latexFromResourceName(const QString &name);
};

#endif // FORMULARENDERER_H
