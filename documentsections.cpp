#include "documentsections.h"

#include "pagedocumentpainter.h"

#include <QColor>
#include <QObject>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace DocumentSections {

QList<Section> sectionsFromDocument(const QTextDocument *document,
                                    const PageLayoutSettings &defaultLayout,
                                    const HeaderFooterSettings &defaultHeaderFooter)
{
    QList<Section> sections;
    sections.append(Section{
        .startPosition = 0,
        .name = QObject::tr("第 1 节"),
        .layout = defaultLayout,
        .headerFooter = defaultHeaderFooter,
    });

    if (!document)
        return sections;

    int index = 2;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat fmt = block.blockFormat();
        if (!fmt.hasProperty(SectionBreakProperty))
            continue;
        if (block.position() <= 0)
            continue;
        QString name = fmt.property(SectionBreakProperty).toString();
        if (name.isEmpty() || name == QLatin1String("0"))
            name = QObject::tr("第 %1 节").arg(index);
        sections.append(Section{
            .startPosition = block.position(),
            .name = name,
            .layout = defaultLayout,
            .headerFooter = defaultHeaderFooter,
        });
        ++index;
    }
    return sections;
}

Section sectionAtPosition(const QList<Section> &sections, int position)
{
    Section current = sections.isEmpty() ? Section{} : sections.first();
    for (const Section &s : sections) {
        if (s.startPosition <= position)
            current = s;
        else
            break;
    }
    return current;
}

Section sectionForPage(const QList<Section> &sections,
                       QTextDocument *document,
                       const PageLayoutSettings &fallbackLayout,
                       int pageIndex)
{
    if (!document || sections.isEmpty()) {
        Section s;
        s.layout = fallbackLayout;
        return s;
    }

    // Approximate: find a character near the top of the page.
    const QRectF content = PageDocumentPainter::contentRectPoints(fallbackLayout);
    document->setPageSize(content.size());
    document->setDocumentMargin(0);
    const qreal y = pageIndex * content.height() + 1.0;
    const int pos = document->documentLayout()->hitTest(QPointF(content.width() / 2.0, y),
                                                        Qt::FuzzyHit);
    if (pos < 0)
        return sections.last();
    return sectionAtPosition(sections, pos);
}

void insertSectionBreak(QTextCursor &cursor, const QString &name)
{
    cursor.beginEditBlock();
    cursor.insertBlock();
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    fmt.setProperty(SectionBreakProperty, name.isEmpty() ? QStringLiteral("section") : name);
    cursor.setBlockFormat(fmt);
    cursor.insertText(QObject::tr("—— 分节符（下一节）——"));
    QTextCharFormat charFmt;
    charFmt.setForeground(QColor(140, 140, 140));
    charFmt.setFontItalic(true);
    charFmt.setFontPointSize(10);
    cursor.mergeCharFormat(charFmt);
    cursor.insertBlock();
    QTextBlockFormat body = cursor.blockFormat();
    body.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
    body.clearProperty(SectionBreakProperty);
    cursor.setBlockFormat(body);
    cursor.endEditBlock();
}

} // namespace DocumentSections
