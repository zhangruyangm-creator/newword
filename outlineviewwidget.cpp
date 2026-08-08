#include "outlineviewwidget.h"
#include "appstyle.h"
#include "styleutils.h"

#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

OutlineViewWidget::OutlineViewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *hint = new QLabel(tr("大纲视图：编辑标题、升降级别。正文内容在其他视图中显示。"), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("outlineViewHint"));
    hint->setStyleSheet(QStringLiteral(
        "color: %1; padding: 10px 12px; background: %2; border-bottom: 1px solid %3;")
                            .arg(AppStyle::textMuted(),
                                 AppStyle::accentSoft(),
                                 AppStyle::border()));

    auto *bar = new QToolBar(this);
    bar->setObjectName(QStringLiteral("outlineViewBar"));
    bar->setIconSize(QSize(16, 16));
    bar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border-bottom: 1px solid %2; spacing: 4px; padding: 4px; }"
        "QToolButton { padding: 4px 8px; border-radius: 4px; }"
        "QToolButton:hover { background: %3; }")
                           .arg(AppStyle::surfaceRaised(),
                                AppStyle::border(),
                                AppStyle::accentSoft()));
    bar->addAction(tr("升级"), this, &OutlineViewWidget::promote);
    bar->addAction(tr("降级"), this, &OutlineViewWidget::demote);
    bar->addSeparator();
    bar->addAction(tr("上移"), this, &OutlineViewWidget::moveUp);
    bar->addAction(tr("下移"), this, &OutlineViewWidget::moveDown);
    bar->addSeparator();
    bar->addAction(tr("全部展开"), this, &OutlineViewWidget::expandAll);
    bar->addAction(tr("全部折叠"), this, &OutlineViewWidget::collapseAll);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("标题"), tr("级别")});
    m_tree->setColumnWidth(0, 420);
    m_tree->setIndentation(18);
    m_tree->setUniformRowHeights(true);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { border: none; background: %1; font-size: 14px; color: %2; }"
        "QTreeWidget::item { padding: 6px 8px; border-radius: 4px; }"
        "QTreeWidget::item:hover { background: %3; }"
        "QTreeWidget::item:selected { background: %3; color: %4; }")
                              .arg(AppStyle::surfaceRaised(),
                                   AppStyle::text(),
                                   AppStyle::accentSoft(),
                                   AppStyle::accent()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(hint);
    layout->addWidget(bar);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemChanged, this, &OutlineViewWidget::onItemChanged);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *, QTreeWidgetItem *) {
        onCurrentChanged();
    });
}

void OutlineViewWidget::refreshTheme()
{
    if (auto *hint = findChild<QLabel *>(QStringLiteral("outlineViewHint"))) {
        hint->setStyleSheet(QStringLiteral(
            "color: %1; padding: 10px 12px; background: %2; border-bottom: 1px solid %3;")
                                .arg(AppStyle::textMuted(),
                                     AppStyle::accentSoft(),
                                     AppStyle::border()));
    }
    if (auto *bar = findChild<QToolBar *>(QStringLiteral("outlineViewBar"))) {
        bar->setStyleSheet(QStringLiteral(
            "QToolBar { background: %1; border-bottom: 1px solid %2; spacing: 4px; padding: 4px; }"
            "QToolButton { padding: 4px 8px; border-radius: 4px; }"
            "QToolButton:hover { background: %3; }")
                               .arg(AppStyle::surfaceRaised(),
                                    AppStyle::border(),
                                    AppStyle::accentSoft()));
    }
    if (m_tree) {
        m_tree->setStyleSheet(QStringLiteral(
            "QTreeWidget { border: none; background: %1; font-size: 14px; color: %2; }"
            "QTreeWidget::item { padding: 6px 8px; border-radius: 4px; }"
            "QTreeWidget::item:hover { background: %3; }"
            "QTreeWidget::item:selected { background: %3; color: %4; }")
                                  .arg(AppStyle::surfaceRaised(),
                                       AppStyle::text(),
                                       AppStyle::accentSoft(),
                                       AppStyle::accent()));
    }
}

void OutlineViewWidget::setDocument(QTextDocument *document)
{
    m_document = document;
    refresh();
}

QTextBlock OutlineViewWidget::blockByNumber(int number) const
{
    if (!m_document || number < 0)
        return {};
    QTextBlock block = m_document->findBlockByNumber(number);
    return block;
}

QList<OutlineViewWidget::HeadingRef> OutlineViewWidget::collectHeadings() const
{
    QList<HeadingRef> list;
    if (!m_document)
        return list;
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0)
            continue;
        HeadingRef ref;
        ref.blockNumber = block.blockNumber();
        ref.level = level;
        ref.text = block.text();
        list.append(ref);
    }
    return list;
}

void OutlineViewWidget::refresh()
{
    if (!m_tree)
        return;
    m_updating = true;
    m_tree->clear();

    QTreeWidgetItem *h1 = nullptr;
    QTreeWidgetItem *h2 = nullptr;

    const auto headings = collectHeadings();
    for (const HeadingRef &ref : headings) {
        auto *item = new QTreeWidgetItem;
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, ref.text);
        item->setText(1, tr("标题 %1").arg(ref.level));
        item->setData(0, Qt::UserRole, ref.blockNumber);
        item->setData(0, Qt::UserRole + 1, ref.level);

        QFont font = item->font(0);
        if (ref.level == 1) {
            font.setPointSize(16);
            font.setBold(true);
        } else if (ref.level == 2) {
            font.setPointSize(14);
            font.setBold(true);
        } else {
            font.setPointSize(12);
            font.setBold(false);
        }
        item->setFont(0, font);

        if (ref.level == 1) {
            m_tree->addTopLevelItem(item);
            h1 = item;
            h2 = nullptr;
        } else if (ref.level == 2) {
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
    m_updating = false;

    if (headings.isEmpty()) {
        auto *empty = new QTreeWidgetItem(m_tree);
        empty->setFlags(Qt::NoItemFlags);
        empty->setText(0, tr("（没有标题。请先在页面/草稿视图中应用标题样式。）"));
        empty->setForeground(0, QColor(140, 140, 140));
    }
}

void OutlineViewWidget::applyHeadingLevel(int blockNumber, int level)
{
    QTextBlock block = blockByNumber(blockNumber);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    StyleUtils::applyHeadingLevel(cursor, qBound(1, level, 4));
    emit documentEdited();
}

void OutlineViewWidget::rewriteHeadingText(int blockNumber, const QString &text)
{
    QTextBlock block = blockByNumber(blockNumber);
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::BlockUnderCursor);
    // Keep block format; replace text only
    const QTextBlockFormat fmt = block.blockFormat();
    const QTextCharFormat charFmt = cursor.charFormat();
    cursor.insertText(text, charFmt);
    cursor.mergeBlockFormat(fmt);
    emit documentEdited();
}

int OutlineViewWidget::currentBlockNumber() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return -1;
    return item->data(0, Qt::UserRole).toInt();
}

void OutlineViewWidget::promote()
{
    const int blockNumber = currentBlockNumber();
    if (blockNumber < 0)
        return;
    QTreeWidgetItem *item = m_tree->currentItem();
    const int level = item->data(0, Qt::UserRole + 1).toInt();
    if (level <= 1)
        return;
    applyHeadingLevel(blockNumber, level - 1);
    refresh();
    selectBlock(blockNumber);
}

void OutlineViewWidget::demote()
{
    const int blockNumber = currentBlockNumber();
    if (blockNumber < 0)
        return;
    QTreeWidgetItem *item = m_tree->currentItem();
    const int level = item->data(0, Qt::UserRole + 1).toInt();
    if (level >= 4)
        return;
    applyHeadingLevel(blockNumber, level + 1);
    refresh();
    selectBlock(blockNumber);
}

void OutlineViewWidget::moveUp()
{
    const int blockNumber = currentBlockNumber();
    if (blockNumber < 0 || !m_document)
        return;

    QTextBlock block = blockByNumber(blockNumber);
    QTextBlock prev = block.previous();
    while (prev.isValid() && prev.blockFormat().headingLevel() <= 0)
        prev = prev.previous();
    if (!prev.isValid())
        return;

    // Swap block contents with previous heading block via cursors
    QTextCursor c1(prev);
    c1.select(QTextCursor::BlockUnderCursor);
    const QString t1 = c1.selectedText();
    const QTextBlockFormat f1 = prev.blockFormat();
    const QTextCharFormat cf1 = c1.charFormat();

    QTextCursor c2(block);
    c2.select(QTextCursor::BlockUnderCursor);
    const QString t2 = c2.selectedText();
    const QTextBlockFormat f2 = block.blockFormat();
    const QTextCharFormat cf2 = c2.charFormat();

    c1.beginEditBlock();
    c1.insertText(t2, cf2);
    c1.setBlockFormat(f2);
    c2.insertText(t1, cf1);
    c2.setBlockFormat(f1);
    c1.endEditBlock();

    emit documentEdited();
    refresh();
    selectBlock(prev.blockNumber());
}

void OutlineViewWidget::moveDown()
{
    const int blockNumber = currentBlockNumber();
    if (blockNumber < 0 || !m_document)
        return;

    QTextBlock block = blockByNumber(blockNumber);
    QTextBlock next = block.next();
    while (next.isValid() && next.blockFormat().headingLevel() <= 0)
        next = next.next();
    if (!next.isValid())
        return;

    QTextCursor c1(block);
    c1.select(QTextCursor::BlockUnderCursor);
    const QString t1 = c1.selectedText();
    const QTextBlockFormat f1 = block.blockFormat();
    const QTextCharFormat cf1 = c1.charFormat();

    QTextCursor c2(next);
    c2.select(QTextCursor::BlockUnderCursor);
    const QString t2 = c2.selectedText();
    const QTextBlockFormat f2 = next.blockFormat();
    const QTextCharFormat cf2 = c2.charFormat();

    c1.beginEditBlock();
    c1.insertText(t2, cf2);
    c1.setBlockFormat(f2);
    c2.insertText(t1, cf1);
    c2.setBlockFormat(f1);
    c1.endEditBlock();

    emit documentEdited();
    refresh();
    selectBlock(next.blockNumber());
}

void OutlineViewWidget::expandAll()
{
    m_tree->expandAll();
}

void OutlineViewWidget::collapseAll()
{
    m_tree->collapseAll();
}

void OutlineViewWidget::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_updating || !item || column != 0)
        return;
    const QVariant blockData = item->data(0, Qt::UserRole);
    if (!blockData.isValid())
        return;
    const int blockNumber = blockData.toInt();
    if (blockNumber < 0)
        return;
    rewriteHeadingText(blockNumber, item->text(0));
}

void OutlineViewWidget::onCurrentChanged()
{
}

void OutlineViewWidget::selectBlock(int blockNumber)
{
    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> findItem =
        [&](QTreeWidgetItem *parent) -> QTreeWidgetItem * {
        const int count = parent ? parent->childCount() : m_tree->topLevelItemCount();
        for (int i = 0; i < count; ++i) {
            QTreeWidgetItem *item = parent ? parent->child(i) : m_tree->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toInt() == blockNumber)
                return item;
            if (QTreeWidgetItem *found = findItem(item))
                return found;
        }
        return nullptr;
    };
    if (QTreeWidgetItem *item = findItem(nullptr))
        m_tree->setCurrentItem(item);
}
