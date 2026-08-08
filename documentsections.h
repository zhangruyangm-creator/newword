#ifndef DOCUMENTSECTIONS_H
#define DOCUMENTSECTIONS_H

#include "headerfootersettings.h"
#include "pagelayout.h"

#include <QList>
#include <QString>
#include <QTextFormat>

class QTextCursor;
class QTextDocument;

namespace DocumentSections {

/** Block property: non-zero means this block starts a new section. */
constexpr int SectionBreakProperty = QTextFormat::UserProperty + 100;

struct Section
{
    int startPosition = 0; // document position of first block in section
    QString name;
    PageLayoutSettings layout;
    HeaderFooterSettings headerFooter;
};

/** At least one section covering the whole document. */
QList<Section> sectionsFromDocument(const QTextDocument *document,
                                    const PageLayoutSettings &defaultLayout,
                                    const HeaderFooterSettings &defaultHeaderFooter);

Section sectionAtPosition(const QList<Section> &sections, int position);
Section sectionForPage(const QList<Section> &sections,
                       QTextDocument *document,
                       const PageLayoutSettings &fallbackLayout,
                       int pageIndex);

void insertSectionBreak(QTextCursor &cursor, const QString &name = QString());

} // namespace DocumentSections

#endif // DOCUMENTSECTIONS_H
