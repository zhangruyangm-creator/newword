#ifndef REVIEWNOTES_H
#define REVIEWNOTES_H

#include <QList>
#include <QString>
#include <QTextFormat>

class QTextCursor;
class QTextDocument;
class QWidget;

namespace ReviewNotes {

constexpr int FootnoteIdProperty = QTextFormat::UserProperty + 101;
constexpr int CommentIdProperty = QTextFormat::UserProperty + 102;
constexpr int EndnoteIdProperty = QTextFormat::UserProperty + 103;

struct Footnote
{
    QString id;
    int markerPosition = -1;
    QString text;
};

struct Endnote
{
    QString id;
    int markerPosition = -1;
    QString text;
};

struct Comment
{
    QString id;
    int start = -1;
    int end = -1;
    QString author;
    QString text;
};

QList<Footnote> collectFootnotes(QTextDocument *document);
QList<Endnote> collectEndnotes(QTextDocument *document);
QList<Comment> collectComments(QTextDocument *document);

/** Insert superscript marker + store footnote body as document meta via trailing section. */
[[nodiscard]] bool insertFootnote(QTextCursor &cursor, const QString &text);

/** Insert superscript marker + store endnote body (document-end appendix). */
[[nodiscard]] bool insertEndnote(QTextCursor &cursor, const QString &text);

/** Highlight selection and attach comment id; body kept in document meta resource. */
[[nodiscard]] bool insertComment(QTextCursor &cursor, const QString &author, const QString &text);

QString footnotesPlainAppendix(QTextDocument *document);
void ensureFootnotesAppendix(QTextDocument *document);
void ensureEndnotesAppendix(QTextDocument *document);

[[nodiscard]] bool promptAndInsertFootnote(QWidget *parent, QTextCursor &cursor);
[[nodiscard]] bool promptAndInsertEndnote(QWidget *parent, QTextCursor &cursor);
[[nodiscard]] bool promptAndInsertComment(QWidget *parent, QTextCursor &cursor);

} // namespace ReviewNotes

#endif // REVIEWNOTES_H
