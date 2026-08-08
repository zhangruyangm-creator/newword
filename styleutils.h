#ifndef STYLEUTILS_H
#define STYLEUTILS_H

#include <QList>
#include <QString>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextFormat>

class QTextCursor;

namespace StyleUtils {

enum class StyleId {
    Normal = 0,
    Title,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Quote
};

constexpr int StyleIdProperty = QTextFormat::UserProperty + 90;

struct StyleInfo
{
    StyleId id;
    QString displayName;
    int headingLevel = 0;
};

QList<StyleInfo> allStyles();
StyleInfo styleInfo(StyleId id);

void applyStyle(QTextCursor &cursor, StyleId id);
StyleId detectStyle(const QTextCursor &cursor);

/** Map heading level 0–4 used by outline promote/demote. */
void applyHeadingLevel(QTextCursor &cursor, int level);

} // namespace StyleUtils

#endif // STYLEUTILS_H
