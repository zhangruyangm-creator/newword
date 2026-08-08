#include "ribbonbar.h"
#include "appstyle.h"

#include <QAction>
#include <QComboBox>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kExpandedHeight = 152;
}

RibbonBar::RibbonBar(QWidget *parent)
    : QWidget(parent)
    , m_expandedHeight(kExpandedHeight)
{
    setObjectName(QStringLiteral("ribbonBar"));
    setStyleSheet(QStringLiteral("#ribbonBar { background: %1; }")
                      .arg(AppStyle::surfaceRaised()));

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setStyleSheet(AppStyle::ribbonExpandedStyleSheet());

    m_collapseButton = new QToolButton(this);
    m_collapseButton->setAutoRaise(true);
    m_collapseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_collapseButton->setFixedSize(28, 28);
    m_collapseButton->setFocusPolicy(Qt::NoFocus);
    m_collapseButton->setStyleSheet(AppStyle::ribbonToolButtonStyleSheet());
    m_tabs->setCornerWidget(m_collapseButton, Qt::TopRightCorner);
    connect(m_collapseButton, &QToolButton::clicked, this, &RibbonBar::toggleCollapsed);
    updateCollapseButton();

    connect(m_tabs, &QTabWidget::tabBarClicked, this, [this](int index) {
        Q_UNUSED(index);
        if (m_collapsed)
            setCollapsed(false);
    });

    if (QTabBar *bar = m_tabs->tabBar()) {
        bar->installEventFilter(this);
        bar->setToolTip(tr("双击折叠/展开功能区"));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);
    setFixedHeight(m_expandedHeight);
}

bool RibbonBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs->tabBar() && event->type() == QEvent::MouseButtonDblClick) {
        toggleCollapsed();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

int RibbonBar::collapsedHeight() const
{
    const int tabH = m_tabs->tabBar() ? m_tabs->tabBar()->sizeHint().height() : 32;
    return qMax(36, tabH + 4);
}

void RibbonBar::setRibbonHeight(int height)
{
    setFixedHeight(height);
}

void RibbonBar::toggleCollapsed()
{
    setCollapsed(!m_collapsed);
}

void RibbonBar::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;
    updateCollapseButton();
    applyCollapsedState(true);
    emit collapsedChanged(m_collapsed);
}

void RibbonBar::updateCollapseButton()
{
    if (!m_collapseButton)
        return;
    if (m_collapsed) {
        m_collapseButton->setArrowType(Qt::DownArrow);
        m_collapseButton->setToolTip(tr("展开功能区 (Ctrl+F1)"));
    } else {
        m_collapseButton->setArrowType(Qt::UpArrow);
        m_collapseButton->setToolTip(tr("折叠功能区 (Ctrl+F1)"));
    }
}

void RibbonBar::refreshTheme()
{
    setStyleSheet(QStringLiteral("#ribbonBar { background: %1; }")
                      .arg(AppStyle::surfaceRaised()));
    if (m_collapsed)
        m_tabs->setStyleSheet(AppStyle::ribbonCollapsedStyleSheet());
    else
        m_tabs->setStyleSheet(AppStyle::ribbonExpandedStyleSheet());
    if (m_collapseButton)
        m_collapseButton->setStyleSheet(AppStyle::ribbonToolButtonStyleSheet());

    const QString groupCss = AppStyle::ribbonGroupStyleSheet();
    const QString titleCss = QStringLiteral(
        "color: %1; font-size: 11px; border: none; background: transparent;")
                                 .arg(AppStyle::textMuted());
    const QString buttonCss = AppStyle::ribbonToolButtonStyleSheet();
    const QString comboCss = AppStyle::ribbonComboStyleSheet();

    for (QFrame *group : findChildren<QFrame *>(QStringLiteral("ribbonGroup")))
        group->setStyleSheet(groupCss);
    for (QLabel *title : findChildren<QLabel *>(QStringLiteral("ribbonGroupTitle")))
        title->setStyleSheet(titleCss);
    for (QToolButton *btn : findChildren<QToolButton *>()) {
        if (btn != m_collapseButton)
            btn->setStyleSheet(buttonCss);
    }
    for (QComboBox *combo : findChildren<QComboBox *>())
        combo->setStyleSheet(comboCss);
}

void RibbonBar::applyCollapsedState(bool animate)
{
    const int target = m_collapsed ? collapsedHeight() : m_expandedHeight;

    // Clip page content when collapsed; restore when expanded.
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (QWidget *page = m_tabs->widget(i)) {
            page->setMaximumHeight(m_collapsed ? 0 : QWIDGETSIZE_MAX);
            page->setVisible(!m_collapsed);
        }
    }

    if (m_collapsed)
        m_tabs->setStyleSheet(AppStyle::ribbonCollapsedStyleSheet());
    else
        m_tabs->setStyleSheet(AppStyle::ribbonExpandedStyleSheet());

    if (!animate) {
        setFixedHeight(target);
        return;
    }

    auto *anim = new QPropertyAnimation(this, "ribbonHeight", this);
    anim->setDuration(160);
    anim->setStartValue(height());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this, target]() {
        setFixedHeight(target);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QWidget *RibbonBar::createGroup(const QString &title, QWidget *content)
{
    auto *box = new QFrame;
    box->setObjectName(QStringLiteral("ribbonGroup"));
    box->setFrameShape(QFrame::NoFrame);
    box->setStyleSheet(AppStyle::ribbonGroupStyleSheet());

    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(10, 8, 10, 6);
    layout->setSpacing(5);
    layout->addWidget(content, 1);

    auto *label = new QLabel(title);
    label->setObjectName(QStringLiteral("ribbonGroupTitle"));
    label->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    label->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; border: none; background: transparent;")
                             .arg(AppStyle::textMuted()));
    layout->addWidget(label);
    return box;
}

QToolButton *RibbonBar::makeButton(QAction *action, Qt::ToolButtonStyle style)
{
    auto *btn = new QToolButton;
    btn->setDefaultAction(action);
    btn->setToolButtonStyle(style);
    btn->setAutoRaise(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(AppStyle::ribbonToolButtonStyleSheet());
    btn->setIconSize(style == Qt::ToolButtonTextUnderIcon ? QSize(26, 26) : QSize(16, 16));
    if (style == Qt::ToolButtonTextUnderIcon)
        btn->setMinimumWidth(58);
    return btn;
}

QWidget *RibbonBar::makeButtonRow(const QList<QAction *> &actions, Qt::ToolButtonStyle style)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    for (QAction *action : actions)
        layout->addWidget(makeButton(action, style));
    layout->addStretch();
    return row;
}

void RibbonBar::addHomeActions(const QList<QAction *> &clipboard,
                               const QList<QAction *> &font,
                               QComboBox *fontCombo,
                               QComboBox *sizeCombo,
                               QComboBox *lineSpacingCombo,
                               const QList<QAction *> &paragraph,
                               const QList<QAction *> &styles)
{
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(0);

    row->addWidget(createGroup(tr("剪贴板"), makeButtonRow(clipboard, Qt::ToolButtonTextUnderIcon)));

    auto *fontPanel = new QWidget;
    auto *fontLayout = new QVBoxLayout(fontPanel);
    fontLayout->setContentsMargins(0, 0, 0, 0);
    fontLayout->setSpacing(4);
    auto *fontTop = new QHBoxLayout;
    fontCombo->setMinimumWidth(140);
    sizeCombo->setMinimumWidth(64);
    fontCombo->setStyleSheet(AppStyle::ribbonComboStyleSheet());
    sizeCombo->setStyleSheet(AppStyle::ribbonComboStyleSheet());
    fontTop->addWidget(fontCombo);
    fontTop->addWidget(sizeCombo);
    fontLayout->addLayout(fontTop);
    fontLayout->addWidget(makeButtonRow(font, Qt::ToolButtonIconOnly));
    row->addWidget(createGroup(tr("字体"), fontPanel));

    auto *paraPanel = new QWidget;
    auto *paraLayout = new QVBoxLayout(paraPanel);
    paraLayout->setContentsMargins(0, 0, 0, 0);
    paraLayout->setSpacing(4);
    lineSpacingCombo->setStyleSheet(AppStyle::ribbonComboStyleSheet());
    paraLayout->addWidget(lineSpacingCombo);
    paraLayout->addWidget(makeButtonRow(paragraph, Qt::ToolButtonIconOnly));
    row->addWidget(createGroup(tr("段落"), paraPanel));

    row->addWidget(createGroup(tr("样式"), makeButtonRow(styles, Qt::ToolButtonTextBesideIcon)));
    row->addStretch();

    m_tabs->addTab(page, tr("开始"));
}

void RibbonBar::addInsertActions(const QList<QAction *> &actions,
                                 const QList<QAction *> &tableActions)
{
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(0);
    row->addWidget(createGroup(tr("插入"), makeButtonRow(actions, Qt::ToolButtonTextUnderIcon)));
    row->addWidget(createGroup(tr("表格"), makeButtonRow(tableActions, Qt::ToolButtonTextUnderIcon)));
    row->addStretch();
    m_tabs->addTab(page, tr("插入"));
}

void RibbonBar::addLayoutActions(const QList<QAction *> &actions)
{
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(0);
    row->addWidget(createGroup(tr("页面设置"), makeButtonRow(actions, Qt::ToolButtonTextUnderIcon)));
    row->addStretch();
    m_tabs->addTab(page, tr("布局"));
}

void RibbonBar::addViewActions(const QList<QAction *> &documentViews,
                               const QList<QAction *> &tools,
                               const QList<QAction *> &themes)
{
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(0);
    row->addWidget(createGroup(tr("文档视图"), makeButtonRow(documentViews, Qt::ToolButtonTextUnderIcon)));
    row->addWidget(createGroup(tr("显示"), makeButtonRow(tools, Qt::ToolButtonTextUnderIcon)));
    if (!themes.isEmpty())
        row->addWidget(createGroup(tr("主题"), makeButtonRow(themes, Qt::ToolButtonTextUnderIcon)));
    row->addStretch();
    m_tabs->addTab(page, tr("视图"));
}
