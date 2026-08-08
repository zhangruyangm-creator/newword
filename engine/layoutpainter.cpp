#include "layoutpainter.h"
#include "pagedocumentpainter.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QTextDocument>
#include <QTextLayout>

namespace Engine {
namespace {

void paintTextLine(QPainter *painter, const LayoutLine &line, const QPointF &origin, qreal minWidth)
{
    if (line.text.isEmpty())
        return;
    QTextLayout tl(line.text, line.baseFont);
    tl.setFormats(line.formats);
    tl.beginLayout();
    QTextLine ql = tl.createLine();
    if (ql.isValid()) {
        ql.setLineWidth(qMax(line.width, minWidth));
        ql.setPosition(QPointF(0, 0));
    }
    tl.endLayout();
    tl.draw(painter, origin);
}

void paintTableRow(QPainter *painter, const LayoutLine &line, const QRectF &content)
{
    qreal x = content.left() + line.x;
    const qreal y = content.top() + line.y;
    const qreal pad = line.tableCellPaddingPt;
    const QPen borderPen(line.tableBorderColor, qMax(0.5, line.tableBorderPt));

    for (const LayoutTableCell &cell : line.tableCells) {
        if (cell.covered) {
            x += cell.width;
            continue;
        }
        const qreal h = cell.paintHeight > 0.5 ? cell.paintHeight : cell.height;
        const QRectF box(x, y, cell.width, h);
        if (cell.background.isValid())
            painter->fillRect(box, cell.background);
        else
            painter->fillRect(box, Qt::white);

        painter->setPen(borderPen);
        painter->drawRect(box);

        painter->setPen(QColor(0, 0, 0));
        const QRectF textBox = box.adjusted(pad, pad, -pad, -pad);
        if (!cell.text.isEmpty() && textBox.width() > 2) {
            QTextLayout tl(cell.text, cell.baseFont);
            tl.setFormats(cell.formats);
            tl.beginLayout();
            qreal ty = 0;
            while (true) {
                QTextLine ql = tl.createLine();
                if (!ql.isValid())
                    break;
                ql.setLineWidth(textBox.width());
                ql.setPosition(QPointF(0, ty));
                ty += ql.height();
            }
            tl.endLayout();
            painter->save();
            painter->setClipRect(textBox);
            tl.draw(painter, textBox.topLeft());
            painter->restore();
        }
        x += cell.width;
    }
}

} // namespace

void paintLayoutPage(QPainter *painter,
                     const LayoutPage &page,
                     const PageLayoutSettings &layout,
                     const HeaderFooterSettings &headerFooter,
                     int totalPages,
                     qreal contentWidthPt,
                     qreal contentHeightPt)
{
    if (!painter)
        return;

    const QSizeF pageSize = PageDocumentPainter::pageSizePoints(layout);
    const QRectF content = PageDocumentPainter::contentRectPoints(layout);
    Q_UNUSED(contentWidthPt);
    Q_UNUSED(contentHeightPt);

    const qreal left = content.left();
    const qreal right = pageSize.width() - content.right();
    const QRectF pageRect(0, 0, pageSize.width(), pageSize.height());

    painter->fillRect(pageRect, Qt::white);
    if (layout.showPageBorder) {
        painter->setPen(QPen(layout.pageBorderColor, layout.pageBorderWidthPt));
        painter->drawRect(pageRect.adjusted(4, 4, -4, -4));
    }

    const int pageIndex = page.index;
    const QString header = headerFooter.headerForPage(pageIndex);
    if (!header.isEmpty()) {
        painter->setPen(QColor(80, 80, 80));
        QFont headerFont = painter->font();
        headerFont.setPointSize(9);
        painter->setFont(headerFont);
        const qreal headerY = PageDocumentPainter::mmToPoints(layout.headerDistanceMm);
        painter->drawText(QRectF(left, headerY, pageSize.width() - left - right, 16),
                          Qt::AlignCenter, header);
    }

    const QString footer = headerFooter.composedFooter(pageIndex, totalPages);
    if (!footer.isEmpty()) {
        painter->setPen(QColor(80, 80, 80));
        QFont footerFont = painter->font();
        footerFont.setPointSize(9);
        painter->setFont(footerFont);
        painter->drawText(QRectF(left,
                                 pageSize.height()
                                     - PageDocumentPainter::mmToPoints(layout.marginsMm.bottom())
                                     - PageDocumentPainter::mmToPoints(layout.footerDistanceMm) + 4,
                                 pageSize.width() - left - right, 16),
                          Qt::AlignCenter, footer);
    }

    painter->save();
    painter->setClipRect(content);
    painter->setPen(QColor(0, 0, 0));
    for (const LayoutLine &line : page.lines) {
        if (line.isTableRow) {
            paintTableRow(painter, line, content);
            continue;
        }
        if (line.isAtomic) {
            const QRectF box(content.left() + line.x,
                             content.top() + line.y,
                             qMax(24.0, line.width > 0 ? line.width : content.width() * 0.5),
                             qMax(12.0, line.height));
            if (!line.image.isNull()) {
                painter->drawImage(box, line.image);
            } else {
                painter->fillRect(box, QColor(245, 245, 245));
                painter->setPen(QPen(QColor(160, 160, 160), 1));
                painter->drawRect(box);
                if (!line.text.isEmpty()) {
                    painter->setPen(QColor(100, 100, 100));
                    painter->drawText(box.adjusted(4, 0, -4, 0),
                                      Qt::AlignVCenter | Qt::AlignLeft, line.text);
                }
                painter->setPen(QColor(0, 0, 0));
            }
            continue;
        }
        paintTextLine(painter, line,
                      QPointF(content.left() + line.x, content.top() + line.y),
                      content.width());
    }

    if (page.hasFootnoteRule) {
        const qreal ruleY = content.top() + page.footnoteRuleY + 6.0;
        painter->setPen(QPen(QColor(80, 80, 80), 0.5));
        painter->drawLine(QPointF(content.left(), ruleY),
                          QPointF(content.left() + qMin(content.width() * 0.35, 120.0), ruleY));
    }
    for (const LayoutLine &line : page.footnoteLines) {
        paintTextLine(painter, line,
                      QPointF(content.left() + line.x, content.top() + line.y),
                      content.width());
    }

    for (const FloatingTextBox &box : page.floatingBoxes) {
        const QRectF rect(content.left() + box.xPt, content.top() + box.yPt, box.wPt, box.hPt);
        if (!box.html.isEmpty()) {
            QTextDocument td;
            td.setHtml(box.html);
            td.setTextWidth(qMax(8.0, rect.width() - 4));
            painter->save();
            painter->translate(rect.left() + 2, rect.top() + 2);
            painter->setClipRect(QRectF(0, 0, rect.width() - 4, rect.height() - 4));
            td.drawContents(painter);
            painter->restore();
        }
    }

    painter->restore();
}

} // namespace Engine
