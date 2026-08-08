#ifndef OUTLINEPANE_H
#define OUTLINEPANE_H

#include <QWidget>

#include <QByteArray>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QTextDocument;
class QToolButton;

class OutlinePane : public QWidget
{
    Q_OBJECT

public:
    explicit OutlinePane(QWidget *parent = nullptr);

    void setDocument(QTextDocument *document);
    void refresh();
    void refreshTheme();

signals:
    void navigateToPosition(int position);
    void closeRequested();

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *m_tree = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QTextDocument *m_document = nullptr;
    QByteArray m_lastFingerprint;
};

#endif // OUTLINEPANE_H
