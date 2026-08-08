#ifndef RULERWIDGET_H
#define RULERWIDGET_H

#include <QMarginsF>
#include <QWidget>

class RulerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RulerWidget(QWidget *parent = nullptr);

    void refreshTheme();
    void setPageWidthPx(int widthPx);
    void setMarginsMm(const QMarginsF &margins);
    void setOffsetPx(int offsetPx);
    void setZoomFactor(qreal factor);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_pageWidthPx = 794;
    int m_offsetPx = 0;
    qreal m_zoomFactor = 1.0;
    QMarginsF m_marginsMm {20, 20, 20, 20};
};

#endif // RULERWIDGET_H
