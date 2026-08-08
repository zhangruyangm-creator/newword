#ifndef HEADERFOOTERSETTINGS_H
#define HEADERFOOTERSETTINGS_H

#include <QString>

struct HeaderFooterSettings
{
    enum class PageNumberFormat {
        ChinesePage,   // 第 N 页
        ChinesePageOf, // 第 N 页 / 共 M 页
        Number,        // N
        NumberSlash,   // N / M
        DashNumber     // - N -
    };

    QString header;
    QString footer;
    bool showPageNumber = true;
    PageNumberFormat pageNumberFormat = PageNumberFormat::ChinesePage;

    bool differentFirstPage = false;
    QString firstHeader;
    QString firstFooter;

    bool differentOddEven = false;
    QString evenHeader;
    QString evenFooter;

    QString headerForPage(int pageIndex) const;
    QString footerBaseForPage(int pageIndex) const;
    QString pageNumberText(int pageIndex, int pageCount) const;
    QString composedFooter(int pageIndex, int pageCount) const;
};

#endif // HEADERFOOTERSETTINGS_H
