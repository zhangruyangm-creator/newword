#include "styleutils.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QTextBlock>
#include <QTextCursor>

#include <algorithm>
#include <ranges>

namespace StyleUtils {

QList<StyleInfo> allStyles()
{
    return {
        {.id = StyleId::Normal, .displayName = QObject::tr("正文"), .headingLevel = 0},
        {.id = StyleId::Title, .displayName = QObject::tr("标题"), .headingLevel = 0},
        {.id = StyleId::Heading1, .displayName = QObject::tr("标题 1"), .headingLevel = 1},
        {.id = StyleId::Heading2, .displayName = QObject::tr("标题 2"), .headingLevel = 2},
        {.id = StyleId::Heading3, .displayName = QObject::tr("标题 3"), .headingLevel = 3},
        {.id = StyleId::Heading4, .displayName = QObject::tr("标题 4"), .headingLevel = 4},
        {.id = StyleId::Quote, .displayName = QObject::tr("引用"), .headingLevel = 0},
    };
}

StyleInfo styleInfo(StyleId id)
{
    const auto styles = allStyles();
    if (const auto it = std::ranges::find(styles, id, &StyleInfo::id); it != styles.end())
        return *it;
    return {.id = StyleId::Normal, .displayName = QObject::tr("正文"), .headingLevel = 0};
}

void applyStyle(QTextCursor &cursor, StyleId id)
{
    cursor.beginEditBlock();
    QTextBlockFormat blockFmt = cursor.blockFormat();
    QTextCharFormat charFmt;
    blockFmt.setProperty(StyleIdProperty, int(id));
    blockFmt.setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    blockFmt.setLeftMargin(0);
    blockFmt.setRightMargin(0);

    using enum StyleId;
    switch (id) {
    case Title:
        blockFmt.setHeadingLevel(0);
        blockFmt.setAlignment(Qt::AlignHCenter);
        blockFmt.setTopMargin(24);
        blockFmt.setBottomMargin(16);
        charFmt.setFontPointSize(28);
        charFmt.setFontWeight(QFont::Bold);
        charFmt.setFontItalic(false);
        break;
    case Heading1:
        blockFmt.setHeadingLevel(1);
        blockFmt.setTopMargin(18);
        blockFmt.setBottomMargin(10);
        charFmt.setFontPointSize(24);
        charFmt.setFontWeight(QFont::Bold);
        charFmt.setFontItalic(false);
        break;
    case Heading2:
        blockFmt.setHeadingLevel(2);
        blockFmt.setTopMargin(14);
        blockFmt.setBottomMargin(8);
        charFmt.setFontPointSize(18);
        charFmt.setFontWeight(QFont::Bold);
        charFmt.setFontItalic(false);
        break;
    case Heading3:
        blockFmt.setHeadingLevel(3);
        blockFmt.setTopMargin(12);
        blockFmt.setBottomMargin(6);
        charFmt.setFontPointSize(14);
        charFmt.setFontWeight(QFont::DemiBold);
        charFmt.setFontItalic(false);
        break;
    case Heading4:
        blockFmt.setHeadingLevel(4);
        blockFmt.setTopMargin(10);
        blockFmt.setBottomMargin(6);
        charFmt.setFontPointSize(13);
        charFmt.setFontWeight(QFont::DemiBold);
        charFmt.setFontItalic(true);
        break;
    case Quote:
        blockFmt.setHeadingLevel(0);
        blockFmt.setLeftMargin(28);
        blockFmt.setRightMargin(28);
        blockFmt.setTopMargin(8);
        blockFmt.setBottomMargin(8);
        charFmt.setFontPointSize(12);
        charFmt.setFontWeight(QFont::Normal);
        charFmt.setFontItalic(true);
        charFmt.setForeground(QColor(QStringLiteral("#555555")));
        break;
    case Normal:
    default:
        blockFmt.setHeadingLevel(0);
        blockFmt.setTopMargin(0);
        blockFmt.setBottomMargin(6);
        charFmt.setFontPointSize(12);
        charFmt.setFontWeight(QFont::Normal);
        charFmt.setFontItalic(false);
        charFmt.setForeground(Qt::black);
        break;
    }

    cursor.mergeBlockFormat(blockFmt);
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.mergeCharFormat(charFmt);
    cursor.clearSelection();
    cursor.setPosition(cursor.block().position());
    cursor.endEditBlock();
}

StyleId detectStyle(const QTextCursor &cursor)
{
    const QTextBlockFormat blockFmt = cursor.blockFormat();
    if (blockFmt.hasProperty(StyleIdProperty)) {
        const int id = blockFmt.intProperty(StyleIdProperty);
        if (id >= int(StyleId::Normal) && id <= int(StyleId::Quote))
            return StyleId(id);
    }
    switch (blockFmt.headingLevel()) {
    case 1: return StyleId::Heading1;
    case 2: return StyleId::Heading2;
    case 3: return StyleId::Heading3;
    case 4: return StyleId::Heading4;
    default: return StyleId::Normal;
    }
}

void applyHeadingLevel(QTextCursor &cursor, int level)
{
    level = qBound(0, level, 4);
    StyleId id = StyleId::Normal;
    switch (level) {
    case 1: id = StyleId::Heading1; break;
    case 2: id = StyleId::Heading2; break;
    case 3: id = StyleId::Heading3; break;
    case 4: id = StyleId::Heading4; break;
    default: id = StyleId::Normal; break;
    }
    applyStyle(cursor, id);
}

} // namespace StyleUtils
