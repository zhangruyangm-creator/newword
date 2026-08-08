#ifndef FINDREPLACEDIALOG_H
#define FINDREPLACEDIALOG_H

#include <QDialog>
#include "pagededitorwidget.h"

class QLineEdit;
class QCheckBox;

class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindReplaceDialog(PagedEditorWidget *editor, QWidget *parent = nullptr);

private slots:
    void findNext();
    void findPrevious();
    void replaceOne();
    void replaceAll();

private:
    bool find(bool forward);

    PagedEditorWidget *m_editor = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_caseSensitive = nullptr;
    QCheckBox *m_wholeWords = nullptr;
};

#endif // FINDREPLACEDIALOG_H
