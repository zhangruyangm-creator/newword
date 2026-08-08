#include "docxcommon.h"
#include "styleutils.h"

namespace DocxCommon {

QString xmlEscape(const QString &text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar &ch : text) {
        switch (ch.unicode()) {
        case '<': out += QLatin1String("&lt;"); break;
        case '>': out += QLatin1String("&gt;"); break;
        case '&': out += QLatin1String("&amp;"); break;
        case '"': out += QLatin1String("&quot;"); break;
        case '\'': out += QLatin1String("&apos;"); break;
        default:
            if (ch.unicode() < 0x20 && ch != QChar::Tabulation && ch != QChar::LineFeed
                && ch != QChar::CarriageReturn)
                continue;
            out += ch;
            break;
        }
    }
    return out;
}

QString styleIdToDocx(StyleUtils::StyleId id)
{
    switch (id) {
    case StyleUtils::StyleId::Title: return QStringLiteral("Title");
    case StyleUtils::StyleId::Heading1: return QStringLiteral("Heading1");
    case StyleUtils::StyleId::Heading2: return QStringLiteral("Heading2");
    case StyleUtils::StyleId::Heading3: return QStringLiteral("Heading3");
    case StyleUtils::StyleId::Heading4: return QStringLiteral("Heading4");
    case StyleUtils::StyleId::Quote: return QStringLiteral("Quote");
    case StyleUtils::StyleId::Normal:
    default: return QStringLiteral("Normal");
    }
}

StyleUtils::StyleId docxStyleToId(const QString &name)
{
    const QString n = name.trimmed();
    if (n.compare(QLatin1String("Title"), Qt::CaseInsensitive) == 0
        || n.compare(QLatin1String("标题"), Qt::CaseInsensitive) == 0)
        return StyleUtils::StyleId::Title;
    if (n.startsWith(QLatin1String("Heading1"), Qt::CaseInsensitive)
        || n == QLatin1String("1") || n.contains(QLatin1String("标题 1"))
        || n.contains(QLatin1String("标题1")) || n.compare(QLatin1String("heading 1"), Qt::CaseInsensitive) == 0)
        return StyleUtils::StyleId::Heading1;
    if (n.startsWith(QLatin1String("Heading2"), Qt::CaseInsensitive)
        || n == QLatin1String("2") || n.contains(QLatin1String("标题 2"))
        || n.contains(QLatin1String("标题2")))
        return StyleUtils::StyleId::Heading2;
    if (n.startsWith(QLatin1String("Heading3"), Qt::CaseInsensitive)
        || n == QLatin1String("3") || n.contains(QLatin1String("标题 3"))
        || n.contains(QLatin1String("标题3")))
        return StyleUtils::StyleId::Heading3;
    if (n.startsWith(QLatin1String("Heading4"), Qt::CaseInsensitive)
        || n == QLatin1String("4") || n.contains(QLatin1String("标题 4"))
        || n.contains(QLatin1String("标题4")))
        return StyleUtils::StyleId::Heading4;
    if (n.compare(QLatin1String("Quote"), Qt::CaseInsensitive) == 0
        || n.contains(QLatin1String("引用")))
        return StyleUtils::StyleId::Quote;
    return StyleUtils::StyleId::Normal;
}

} // namespace DocxCommon
