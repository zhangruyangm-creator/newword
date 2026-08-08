#include "rulerwidget.h"
#include "appstyle.h"

#include <QPainter>
#include <QtGlobal>

namespace {
constexpr qreal kMmToPx = 96.0 / 25.4;
}

RulerWidget::RulerWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("rulerWidget"));
    setFixedHeight(24);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(AppStyle::rulerStyleSheet());
}

void RulerWidget::refreshTheme()
{
    setStyleSheet(AppStyle::rulerStyleSheet());
}

void RulerWidget::setPageWidthPx(int widthPx)
{
    m_pageWidthPx = widthPx;
    update();
}

void RulerWidget::setMarginsMm(const QMarginsF &margins)
{
    m_marginsMm = margins;
    update();
}

void RulerWidget::setOffsetPx(int offsetPx)
{
    m_offsetPx = offsetPx;
    update();
}

void RulerWidget::setZoomFactor(qreal factor)
{
    factor = qMax<qreal>(0.25, factor);
    if (qFuzzyCompare(factor, m_zoomFactor))
        return;
    m_zoomFactor = factor;
    update();
}

void RulerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), QColor(247, 248, 250));
    p.setPen(QColor(208, 213, 221));
    p.drawLine(0, height() - 1, width(), height() - 1);

    const qreal pxPerMm = kMmToPx * m_zoomFactor;
    const int left = m_offsetPx;
    const int right = left + m_pageWidthPx;
    const int marginL = left + qRound(m_marginsMm.left() * pxPerMm);
    const int marginR = right - qRound(m_marginsMm.right() * pxPerMm);

    p.fillRect(QRect(left, 0, m_pageWidthPx, height() - 1), QColor(255, 255, 255));
    p.fillRect(QRect(left, 0, marginL - left, height() - 1), QColor(236, 239, 243));
    p.fillRect(QRect(marginR, 0, right - marginR, height() - 1), QColor(236, 239, 243));

    p.setPen(QColor(26, 95, 180));
    p.drawLine(marginL, 2, marginL, height() - 3);
    p.drawLine(marginR, 2, marginR, height() - 3);

    p.setPen(QColor(107, 114, 128));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);

    const qreal pageWidthMm = m_pageWidthPx / pxPerMm;
    for (int cm = 0; cm <= static_cast<int>(pageWidthMm / 10.0) + 1; ++cm) {
        const int x = left + qRound(cm * 10.0 * pxPerMm);
        if (x > right)
            break;
        p.drawLine(x, height() - 8, x, height() - 2);
        if (cm > 0)
            p.drawText(QRect(x - 10, 1, 20, 12), Qt::AlignCenter, QString::number(cm));
        for (int mm = 1; mm < 10; ++mm) {
            const int sx = left + qRound((cm * 10.0 + mm) * pxPerMm);
            if (sx > right)
                break;
            const int tick = (mm == 5) ? 6 : 4;
            p.drawLine(sx, height() - tick, sx, height() - 2);
        }
    }
}
