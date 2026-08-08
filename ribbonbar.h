#ifndef RIBBONBAR_H
#define RIBBONBAR_H

#include <QWidget>

class QAction;
class QComboBox;
class QHBoxLayout;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

class RibbonBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int ribbonHeight READ ribbonHeight WRITE setRibbonHeight)

public:
    explicit RibbonBar(QWidget *parent = nullptr);

    void addHomeActions(const QList<QAction *> &clipboard,
                        const QList<QAction *> &font,
                        QComboBox *fontCombo,
                        QComboBox *sizeCombo,
                        QComboBox *lineSpacingCombo,
                        const QList<QAction *> &paragraph,
                        const QList<QAction *> &styles);

    void addInsertActions(const QList<QAction *> &actions,
                          const QList<QAction *> &tableActions);

    void addLayoutActions(const QList<QAction *> &actions);

    void addViewActions(const QList<QAction *> &documentViews,
                        const QList<QAction *> &tools,
                        const QList<QAction *> &themes = {});

    void refreshTheme();

    [[nodiscard]] bool isCollapsed() const { return m_collapsed; }
    [[nodiscard]] int ribbonHeight() const { return height(); }

public slots:
    void setCollapsed(bool collapsed);
    void toggleCollapsed();
    void setRibbonHeight(int height);

signals:
    void collapsedChanged(bool collapsed);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *createGroup(const QString &title, QWidget *content);
    QToolButton *makeButton(QAction *action, Qt::ToolButtonStyle style = Qt::ToolButtonTextUnderIcon);
    QWidget *makeButtonRow(const QList<QAction *> &actions, Qt::ToolButtonStyle style);
    void applyCollapsedState(bool animate);
    void updateCollapseButton();
    [[nodiscard]] int expandedHeight() const { return m_expandedHeight; }
    [[nodiscard]] int collapsedHeight() const;

    QTabWidget *m_tabs = nullptr;
    QToolButton *m_collapseButton = nullptr;
    bool m_collapsed = false;
    int m_expandedHeight = 148;
};

#endif // RIBBONBAR_H
