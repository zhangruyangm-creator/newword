#ifndef OUTLINEVIEWWIDGET_H
#define OUTLINEVIEWWIDGET_H

#include <QWidget>
#include <QTextBlock>

class QTreeWidget;
class QTreeWidgetItem;
class QTextDocument;
class QToolBar;

class OutlineViewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OutlineViewWidget(QWidget *parent = nullptr);

    void setDocument(QTextDocument *document);
    void refresh();
    void refreshTheme();

signals:
    void documentEdited();
    void requestFocusEditor();

private slots:
    void promote();
    void demote();
    void moveUp();
    void moveDown();
    void expandAll();
    void collapseAll();
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onCurrentChanged();

private:
    struct HeadingRef {
        int blockNumber = -1;
        int level = 0;
        QString text;
    };

    QList<HeadingRef> collectHeadings() const;
    void applyHeadingLevel(int blockNumber, int level);
    void rewriteHeadingText(int blockNumber, const QString &text);
    QTextBlock blockByNumber(int number) const;
    int currentBlockNumber() const;
    void selectBlock(int blockNumber);

    QTreeWidget *m_tree = nullptr;
    QTextDocument *m_document = nullptr;
    bool m_updating = false;
};

#endif // OUTLINEVIEWWIDGET_H
