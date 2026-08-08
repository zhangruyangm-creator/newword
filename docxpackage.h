#ifndef DOCXPACKAGE_H
#define DOCXPACKAGE_H

#include "docxmeta.h"
#include "floatingtextbox.h"

#include <QString>
#include <QVector>

/** Read/write DOCX chrome (headers, footers, page size) without touching body HTML. */
namespace DocxPackage {

[[nodiscard]] bool readMeta(const QString &filePath, DocxDocumentMeta *meta);
/** After body save: inject/replace header1/footer1 + sectPr page metrics. */
[[nodiscard]] bool applyMeta(const QString &filePath, const DocxDocumentMeta &meta,
                             QString *errorMessage = nullptr);

//! True when package contains at least one real footnote (id > 0).
[[nodiscard]] bool hasFootnotes(const QString &filePath);
//! True when package contains at least one real endnote (id > 0).
[[nodiscard]] bool hasEndnotes(const QString &filePath);
//! True when package contains word/comments.xml with at least one comment.
[[nodiscard]] bool hasComments(const QString &filePath);

//! NewWord floating text boxes (customXml part; ignored by Word).
[[nodiscard]] QVector<FloatingTextBox> readFloatingBoxes(const QString &filePath);
[[nodiscard]] bool writeFloatingBoxes(const QString &filePath,
                                      const QVector<FloatingTextBox> &boxes,
                                      QString *errorMessage = nullptr);

} // namespace DocxPackage

#endif // DOCXPACKAGE_H
