#ifndef PAGELAYOUT_H
#define PAGELAYOUT_H

#include <QColor>
#include <QMarginsF>
#include <QSizeF>
#include <QString>

struct PageLayoutSettings
{
    enum class Paper {
        A4,
        A5,
        Letter,
        Legal,
        Custom
    };

    enum class Orientation {
        Portrait,
        Landscape
    };

    Paper paper = Paper::A4;
    Orientation orientation = Orientation::Portrait;
    qreal customWidthMm = 210.0;
    qreal customHeightMm = 297.0;

    QMarginsF marginsMm {25.4, 25.4, 25.4, 25.4};
    int columnCount = 1;
    qreal columnSpacingMm = 10.0;

    bool showPageBorder = false;
    qreal pageBorderWidthPt = 1.0;
    QColor pageBorderColor {80, 80, 80};

    qreal headerDistanceMm = 12.0;
    qreal footerDistanceMm = 12.0;

    QSizeF pageSizeMm() const;
    QString paperName() const;

    static QSizeF sizeForPaper(Paper paper);
    static PageLayoutSettings narrowMargins();
    static PageLayoutSettings normalMargins();
    static PageLayoutSettings moderateMargins();
    static PageLayoutSettings wideMargins();
};

#endif // PAGELAYOUT_H
