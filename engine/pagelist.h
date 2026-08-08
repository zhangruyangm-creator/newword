#ifndef ENGINE_PAGELIST_H
#define ENGINE_PAGELIST_H

#include "../floatingtextbox.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QString>
#include <QTextLayout>
#include <QVector>

namespace Engine {

struct LayoutTableCell {
    QString text;
    QFont baseFont;
    QVector<QTextLayout::FormatRange> formats;
    QColor background;
    qreal width = 0;
    qreal height = 0;      //!< height of this layout row strip
    qreal paintHeight = 0; //!< may span multiple rows (rowSpan)
    int columnSpan = 1;
    int rowSpan = 1;
    bool covered = false; //!< skip paint; still advances x
};

struct LayoutLine {
    QString text;
    QFont baseFont;
    QVector<QTextLayout::FormatRange> formats;
    qreal x = 0;
    qreal y = 0; // top of line within page content box
    qreal width = 0;
    qreal height = 0;
    qreal ascent = 0;
    bool isAtomic = false;
    QImage image;

    bool isTableRow = false;
    QVector<LayoutTableCell> tableCells;
    qreal tableBorderPt = 0.5;
    QColor tableBorderColor {160, 160, 160};
    qreal tableCellPaddingPt = 4.0;
};

struct LayoutPage {
    int index = 0;
    int startDocPos = -1; //!< First content's QTextDocument position on this page
    QVector<LayoutLine> lines;
    //! Footnote lines drawn in the bottom band (y relative to content box).
    QVector<LayoutLine> footnoteLines;
    bool hasFootnoteRule = false;
    qreal footnoteRuleY = 0; //!< top of separator within content box
    //! Absolute boxes for this page (coords relative to content box).
    QVector<FloatingTextBox> floatingBoxes;
};

struct LayoutResult {
    QVector<LayoutPage> pages;
    qreal contentWidthPt = 0;
    qreal contentHeightPt = 0;

    [[nodiscard]] int pageCount() const { return qMax(1, pages.size()); }

    //! Document positions where a new page begins (pages 1..n-1) — for live page seams.
    [[nodiscard]] QVector<int> pageBreakDocPositions() const
    {
        QVector<int> out;
        for (int i = 1; i < pages.size(); ++i) {
            if (pages.at(i).startDocPos >= 0)
                out.append(pages.at(i).startDocPos);
        }
        return out;
    }
};

} // namespace Engine

#endif // ENGINE_PAGELIST_H
