#include "headerfootersettings.h"

#include <QtGlobal>

QString HeaderFooterSettings::headerForPage(int pageIndex) const
{
    if (pageIndex < 0)
        pageIndex = 0;
    if (differentFirstPage && pageIndex == 0)
        return firstHeader;
    if (differentOddEven) {
        // pageIndex 0 = page 1 = odd
        const bool odd = ((pageIndex + 1) % 2) == 1;
        return odd ? header : evenHeader;
    }
    return header;
}

QString HeaderFooterSettings::footerBaseForPage(int pageIndex) const
{
    if (pageIndex < 0)
        pageIndex = 0;
    if (differentFirstPage && pageIndex == 0)
        return firstFooter;
    if (differentOddEven) {
        const bool odd = ((pageIndex + 1) % 2) == 1;
        return odd ? footer : evenFooter;
    }
    return footer;
}

QString HeaderFooterSettings::pageNumberText(int pageIndex, int pageCount) const
{
    if (!showPageNumber)
        return {};
    const int n = pageIndex + 1;
    const int m = qMax(1, pageCount);
    using enum PageNumberFormat;
    switch (pageNumberFormat) {
    case ChinesePageOf:
        return QStringLiteral("第 %1 页 / 共 %2 页").arg(n).arg(m);
    case Number:
        return QString::number(n);
    case NumberSlash:
        return QStringLiteral("%1 / %2").arg(n).arg(m);
    case DashNumber:
        return QStringLiteral("- %1 -").arg(n);
    case ChinesePage:
    default:
        return QStringLiteral("第 %1 页").arg(n);
    }
}

QString HeaderFooterSettings::composedFooter(int pageIndex, int pageCount) const
{
    const QString base = footerBaseForPage(pageIndex);
    const QString page = pageNumberText(pageIndex, pageCount);
    if (page.isEmpty())
        return base;
    if (base.isEmpty())
        return page;
    return base + QStringLiteral("  ·  ") + page;
}
