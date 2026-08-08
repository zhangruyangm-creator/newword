#include "outlinepane.h"
#include "appstyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextBlock>
#include <QTextDocument>
#include <QVariant>
#include <QVBoxLayout>

OutlinePane::OutlinePane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("outlinePane"));
    setMinimumWidth(180);
    setMaximumWidth(360);
    setStyleSheet(AppStyle::outlinePaneStyleSheet());

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("outlineHeader"));
    header->setFixedHeight(34);

    auto *title = new QLabel(tr("导航"), header);
    title->setObjectName(QStringLiteral("outlineTitle"));

    auto *closeBtn = new QToolButton(header);
    closeBtn->setObjectName(QStringLiteral("outlineClose"));
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setToolTip(tr("关闭导航窗格"));
    connect(closeBtn, &QToolButton::clicked, this, &OutlinePane::closeRequested);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(true);

    m_emptyLabel = new QLabel(tr("使用「标题 1/2/3」样式后，这里会显示文档大纲。"), this);
    m_emptyLabel->setObjectName(QStringLiteral("outlineEmpty"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 4, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(m_tree, 1);
    bodyLayout->addWidget(m_emptyLabel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(body, 1);

    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlinePane::onItemClicked);
    m_tree->hide();
    m_emptyLabel->show();
}

void OutlinePane::setDocument(QTextDocument *document)
{
    if (m_document != document)
        m_lastFingerprint.clear();
    m_document = document;
    refresh();
}

void OutlinePane::refresh()
{
    if (!m_document) {
        m_tree->clear();
        m_lastFingerprint.clear();
        m_tree->hide();
        m_emptyLabel->show();
        return;
    }

    // Skip rebuild when heading structure is unchanged (most body typing).
    QByteArray fingerprint;
    fingerprint.reserve(256);
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0)
            continue;
        const QString text = block.text().trimmed();
        if (text.isEmpty())
            continue;
        fingerprint.append(char('0' + qBound(0, level, 9)));
        fingerprint.append(char(0x1F));
        fingerprint.append(QByteArray::number(block.position()));
        fingerprint.append(char(0x1F));
        fingerprint.append(text.toUtf8());
        fingerprint.append(char(0x1E));
    }
    if (!m_lastFingerprint.isEmpty() && fingerprint == m_lastFingerprint)
        return;
    m_lastFingerprint = fingerprint;

    m_tree->clear();

    QTreeWidgetItem *h1 = nullptr;
    QTreeWidgetItem *h2 = nullptr;
    int count = 0;

    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0)
            continue;
        const QString text = block.text().trimmed();
        if (text.isEmpty())
            continue;

        auto *item = new QTreeWidgetItem;
        item->setText(0, text);
        item->setData(0, Qt::UserRole, block.position());
        item->setToolTip(0, text);
        ++count;

        if (level == 1) {
            m_tree->addTopLevelItem(item);
            h1 = item;
            h2 = nullptr;
        } else if (level == 2) {
            if (h1)
                h1->addChild(item);
            else
                m_tree->addTopLevelItem(item);
            h2 = item;
        } else {
            if (h2)
                h2->addChild(item);
            else if (h1)
                h1->addChild(item);
            else
                m_tree->addTopLevelItem(item);
        }
    }

    m_tree->expandAll();

    if (count == 0) {
        m_tree->hide();
        m_emptyLabel->show();
    } else {
        m_emptyLabel->hide();
        m_tree->show();
    }
}

void OutlinePane::onItemClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;
    const QVariant pos = item->data(0, Qt::UserRole);
    if (pos.isValid())
        emit navigateToPosition(pos.toInt());
}

void OutlinePane::refreshTheme()
{
    setStyleSheet(AppStyle::outlinePaneStyleSheet());
}
