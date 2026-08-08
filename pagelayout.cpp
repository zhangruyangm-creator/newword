#include "pagelayout.h"

QSizeF PageLayoutSettings::sizeForPaper(Paper paper)
{
    using enum Paper;
    switch (paper) {
    case A5:
        return {148.0, 210.0};
    case Letter:
        return {215.9, 279.4};
    case Legal:
        return {215.9, 355.6};
    case Custom:
        return {210.0, 297.0};
    case A4:
    default:
        return {210.0, 297.0};
    }
}

QSizeF PageLayoutSettings::pageSizeMm() const
{
    QSizeF size = (paper == Paper::Custom)
        ? QSizeF(customWidthMm, customHeightMm)
        : sizeForPaper(paper);

    if (orientation == Orientation::Landscape)
        size.transpose();
    return size;
}

QString PageLayoutSettings::paperName() const
{
    using enum Paper;
    switch (paper) {
    case A5:
        return QStringLiteral("A5");
    case Letter:
        return QStringLiteral("Letter");
    case Legal:
        return QStringLiteral("Legal");
    case Custom:
        return QStringLiteral("Custom");
    case A4:
    default:
        return QStringLiteral("A4");
    }
}

PageLayoutSettings PageLayoutSettings::narrowMargins()
{
    return PageLayoutSettings{.marginsMm = {12.7, 12.7, 12.7, 12.7}};
}

PageLayoutSettings PageLayoutSettings::normalMargins()
{
    return PageLayoutSettings{.marginsMm = {25.4, 25.4, 25.4, 25.4}};
}

PageLayoutSettings PageLayoutSettings::moderateMargins()
{
    return PageLayoutSettings{.marginsMm = {19.05, 25.4, 19.05, 25.4}};
}

PageLayoutSettings PageLayoutSettings::wideMargins()
{
    return PageLayoutSettings{.marginsMm = {50.8, 50.8, 50.8, 50.8}};
}
