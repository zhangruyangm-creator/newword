#include "findreplacedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

FindReplaceDialog::FindReplaceDialog(PagedEditorWidget *editor, QWidget *parent)
    : QDialog(parent)
    , m_editor(editor)
{
    setWindowTitle(tr("查找和替换"));
    setModal(false);
    resize(420, 180);

    m_findEdit = new QLineEdit(this);
    m_replaceEdit = new QLineEdit(this);
    m_caseSensitive = new QCheckBox(tr("区分大小写"), this);
    m_wholeWords = new QCheckBox(tr("全字匹配"), this);

    auto *form = new QFormLayout;
    form->addRow(tr("查找内容:"), m_findEdit);
    form->addRow(tr("替换为:"), m_replaceEdit);

    auto *options = new QHBoxLayout;
    options->addWidget(m_caseSensitive);
    options->addWidget(m_wholeWords);
    options->addStretch();

    auto *findNextBtn = new QPushButton(tr("查找下一个"), this);
    auto *findPrevBtn = new QPushButton(tr("查找上一个"), this);
    auto *replaceBtn = new QPushButton(tr("替换"), this);
    auto *replaceAllBtn = new QPushButton(tr("全部替换"), this);
    auto *closeBtn = new QPushButton(tr("关闭"), this);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(findNextBtn);
    buttons->addWidget(findPrevBtn);
    buttons->addWidget(replaceBtn);
    buttons->addWidget(replaceAllBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(options);
    layout->addLayout(buttons);

    connect(findNextBtn, &QPushButton::clicked, this, &FindReplaceDialog::findNext);
    connect(findPrevBtn, &QPushButton::clicked, this, &FindReplaceDialog::findPrevious);
    connect(replaceBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceOne);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceDialog::findNext);
}

void FindReplaceDialog::findNext()
{
    if (!find(true))
        QMessageBox::information(this, tr("查找"), tr("未找到匹配内容。"));
}

void FindReplaceDialog::findPrevious()
{
    if (!find(false))
        QMessageBox::information(this, tr("查找"), tr("未找到匹配内容。"));
}

bool FindReplaceDialog::find(bool forward)
{
    const QString text = m_findEdit->text();
    if (text.isEmpty() || !m_editor)
        return false;

    QTextDocument::FindFlags flags;
    if (!forward)
        flags |= QTextDocument::FindBackward;
    if (m_caseSensitive->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWords->isChecked())
        flags |= QTextDocument::FindWholeWords;

    bool found = m_editor->find(text, flags);
    if (!found) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        m_editor->setTextCursor(cursor);
        found = m_editor->find(text, flags);
    }
    return found;
}

void FindReplaceDialog::replaceOne()
{
    if (!m_editor)
        return;

    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()
        && cursor.selectedText() == m_findEdit->text()) {
        cursor.insertText(m_replaceEdit->text());
    }
    findNext();
}

void FindReplaceDialog::replaceAll()
{
    if (!m_editor)
        return;

    const QString findText = m_findEdit->text();
    if (findText.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    if (m_caseSensitive->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWords->isChecked())
        flags |= QTextDocument::FindWholeWords;

    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();

    int count = 0;
    cursor = m_editor->document()->find(findText, 0, flags);
    while (!cursor.isNull()) {
        cursor.insertText(m_replaceEdit->text());
        ++count;
        cursor = m_editor->document()->find(findText, cursor, flags);
    }

    cursor.endEditBlock();
    QMessageBox::information(this, tr("全部替换"),
                             tr("已替换 %1 处。").arg(count));
}
