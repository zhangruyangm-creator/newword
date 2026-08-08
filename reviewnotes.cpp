#include "reviewnotes.h"

#include <QColor>
#include <QFont>
#include <QInputDialog>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

namespace ReviewNotes {
namespace {
const QString kFootnoteResource = QStringLiteral("newword://footnotes");
const QString kEndnoteResource = QStringLiteral("newword://endnotes");
const QString kCommentResource = QStringLiteral("newword://comments");
const QString kFootnoteAppendixHeading = QStringLiteral("脚注");
const QString kEndnoteAppendixHeading = QStringLiteral("尾注");

bool isAppendixHeading(const QTextBlock &block, const QString &title)
{
    return block.isValid() && block.blockFormat().headingLevel() == 2
        && block.text().trimmed() == title;
}

bool isAnyAppendixHeading(const QTextBlock &block)
{
    return isAppendixHeading(block, kFootnoteAppendixHeading)
        || isAppendixHeading(block, kEndnoteAppendixHeading);
}

/** Replace or remove the trailing appendix section titled `title` (H2). */
void replaceAppendix(QTextDocument *document, const QString &title, const QStringList &lines)
{
    if (!document)
        return;

    QTextCursor cur(document);
    cur.beginEditBlock();

    int appendixStart = -1;
    int appendixEnd = -1; // exclusive doc position
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (isAppendixHeading(block, title)) {
            appendixStart = block.position();
            QTextBlock next = block.next();
            while (next.isValid()) {
                if (isAnyAppendixHeading(next) && !isAppendixHeading(next, title))
                    break;
                next = next.next();
            }
            appendixEnd = next.isValid() ? next.position() : document->characterCount() - 1;
            break;
        }
    }

    if (appendixStart >= 0) {
        cur.setPosition(appendixStart);
        cur.setPosition(qMax(appendixStart, appendixEnd), QTextCursor::KeepAnchor);
        cur.removeSelectedText();
    }

    if (lines.isEmpty()) {
        cur.endEditBlock();
        return;
    }

    cur.movePosition(QTextCursor::End);
    // Keep footnotes before endnotes when both exist.
    if (title == kEndnoteAppendixHeading) {
        // already at end
    } else if (title == kFootnoteAppendixHeading) {
        // Insert before existing endnotes appendix if present.
        for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
            if (isAppendixHeading(block, kEndnoteAppendixHeading)) {
                cur.setPosition(block.position());
                break;
            }
        }
    }

    if (cur.position() > 0)
        cur.insertBlock();
    QTextBlockFormat heading;
    heading.setHeadingLevel(2);
    heading.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    cur.setBlockFormat(heading);
    QTextCharFormat headingChar;
    headingChar.setFontWeight(QFont::Bold);
    headingChar.setFontPointSize(14);
    cur.insertText(title, headingChar);

    for (const QString &line : lines) {
        cur.insertBlock();
        QTextBlockFormat body;
        body.setHeadingLevel(0);
        body.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
        cur.setBlockFormat(body);
        cur.insertText(line);
    }
    cur.endEditBlock();
}

} // namespace

QList<Footnote> collectFootnotes(QTextDocument *document)
{
    QList<Footnote> list;
    if (!document)
        return list;
    const QVariantMap map =
        document->resource(QTextDocument::UserResource, QUrl(kFootnoteResource)).toMap();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (isAnyAppendixHeading(block))
            break;
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid())
                continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (!fmt.hasProperty(FootnoteIdProperty))
                continue;
            const QString id = fmt.property(FootnoteIdProperty).toString();
            list.append(Footnote{
                .id = id,
                .markerPosition = frag.position(),
                .text = map.value(id).toString(),
            });
        }
    }
    return list;
}

QList<Endnote> collectEndnotes(QTextDocument *document)
{
    QList<Endnote> list;
    if (!document)
        return list;
    const QVariantMap map =
        document->resource(QTextDocument::UserResource, QUrl(kEndnoteResource)).toMap();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (isAnyAppendixHeading(block))
            break;
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid())
                continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (!fmt.hasProperty(EndnoteIdProperty))
                continue;
            const QString id = fmt.property(EndnoteIdProperty).toString();
            list.append(Endnote{
                .id = id,
                .markerPosition = frag.position(),
                .text = map.value(id).toString(),
            });
        }
    }
    return list;
}

QList<Comment> collectComments(QTextDocument *document)
{
    QList<Comment> list;
    if (!document)
        return list;
    const QVariant v = document->resource(QTextDocument::UserResource, QUrl(kCommentResource));
    const QVariantMap map = v.toMap();
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QVariantMap entry = it.value().toMap();
        list.append(Comment{
            .id = it.key(),
            .start = entry.value(QStringLiteral("start")).toInt(),
            .end = entry.value(QStringLiteral("end")).toInt(),
            .author = entry.value(QStringLiteral("author")).toString(),
            .text = entry.value(QStringLiteral("text")).toString(),
        });
    }
    return list;
}

bool insertFootnote(QTextCursor &cursor, const QString &text)
{
    if (text.trimmed().isEmpty())
        return false;
    QTextDocument *doc = cursor.document();
    if (!doc)
        return false;

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap map = doc->resource(QTextDocument::UserResource, QUrl(kFootnoteResource)).toMap();
    map.insert(id, text.trimmed());
    doc->addResource(QTextDocument::UserResource, QUrl(kFootnoteResource), map);

    const int number = map.size();
    cursor.beginEditBlock();
    QTextCharFormat fmt = cursor.charFormat();
    fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    fmt.setForeground(QColor(20, 90, 180));
    fmt.setProperty(FootnoteIdProperty, id);
    cursor.insertText(QString::number(number), fmt);
    cursor.endEditBlock();

    ensureFootnotesAppendix(doc);
    return true;
}

bool insertEndnote(QTextCursor &cursor, const QString &text)
{
    if (text.trimmed().isEmpty())
        return false;
    QTextDocument *doc = cursor.document();
    if (!doc)
        return false;

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap map = doc->resource(QTextDocument::UserResource, QUrl(kEndnoteResource)).toMap();
    map.insert(id, text.trimmed());
    doc->addResource(QTextDocument::UserResource, QUrl(kEndnoteResource), map);

    const int number = map.size();
    cursor.beginEditBlock();
    QTextCharFormat fmt = cursor.charFormat();
    fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    fmt.setForeground(QColor(120, 60, 20));
    fmt.setProperty(EndnoteIdProperty, id);
    // Roman-style marker in the live editor (i, ii, iii…).
    static const char *kRoman[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x",
                                   "xi", "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii",
                                   "xix", "xx"};
    const QString marker = (number >= 1 && number <= 20)
        ? QString::fromLatin1(kRoman[number - 1])
        : QString::number(number);
    cursor.insertText(marker, fmt);
    cursor.endEditBlock();

    ensureEndnotesAppendix(doc);
    return true;
}

bool insertComment(QTextCursor &cursor, const QString &author, const QString &text)
{
    if (!cursor.hasSelection() || text.trimmed().isEmpty())
        return false;
    QTextDocument *doc = cursor.document();
    if (!doc)
        return false;

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap map = doc->resource(QTextDocument::UserResource, QUrl(kCommentResource)).toMap();
    QVariantMap entry;
    entry.insert(QStringLiteral("start"), cursor.selectionStart());
    entry.insert(QStringLiteral("end"), cursor.selectionEnd());
    entry.insert(QStringLiteral("author"), author);
    entry.insert(QStringLiteral("text"), text.trimmed());
    map.insert(id, entry);
    doc->addResource(QTextDocument::UserResource, QUrl(kCommentResource), map);

    QTextCharFormat fmt;
    fmt.setBackground(QColor(255, 249, 196));
    fmt.setProperty(CommentIdProperty, id);
    cursor.mergeCharFormat(fmt);
    return true;
}

QString footnotesPlainAppendix(QTextDocument *document)
{
    const QList<Footnote> notes = collectFootnotes(document);
    if (notes.isEmpty())
        return {};
    QString out = kFootnoteAppendixHeading + QLatin1Char('\n');
    int i = 1;
    for (const Footnote &fn : notes) {
        out += QStringLiteral("%1. %2\n").arg(i).arg(fn.text);
        ++i;
    }
    return out;
}

void ensureFootnotesAppendix(QTextDocument *document)
{
    const QList<Footnote> notes = collectFootnotes(document);
    QStringList lines;
    int i = 1;
    for (const Footnote &fn : notes) {
        lines.append(QStringLiteral("%1. %2").arg(i).arg(fn.text));
        ++i;
    }
    replaceAppendix(document, kFootnoteAppendixHeading, lines);
}

void ensureEndnotesAppendix(QTextDocument *document)
{
    const QList<Endnote> notes = collectEndnotes(document);
    QStringList lines;
    int i = 1;
    for (const Endnote &en : notes) {
        lines.append(QStringLiteral("%1. %2").arg(i).arg(en.text));
        ++i;
    }
    replaceAppendix(document, kEndnoteAppendixHeading, lines);
}

bool promptAndInsertFootnote(QWidget *parent, QTextCursor &cursor)
{
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        parent, QObject::tr("插入脚注"), QObject::tr("脚注内容："), QString(), &ok);
    if (!ok)
        return false;
    return insertFootnote(cursor, text);
}

bool promptAndInsertEndnote(QWidget *parent, QTextCursor &cursor)
{
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        parent, QObject::tr("插入尾注"), QObject::tr("尾注内容："), QString(), &ok);
    if (!ok)
        return false;
    return insertEndnote(cursor, text);
}

bool promptAndInsertComment(QWidget *parent, QTextCursor &cursor)
{
    if (!cursor.hasSelection())
        return false;
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        parent, QObject::tr("新建批注"), QObject::tr("批注内容："), QString(), &ok);
    if (!ok)
        return false;
    return insertComment(cursor, QObject::tr("审阅者"), text);
}

} // namespace ReviewNotes
