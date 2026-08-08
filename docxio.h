#ifndef DOCXIO_H
#define DOCXIO_H

#include "docxmeta.h"

#include <QString>

class QTextDocument;

/** DOCX round-trip via DocumentModel:
 *  load:  DOCX → DocxImporter → Model → QTextAdapter::toDocument
 *  save:  QTextAdapter::fromDocument → Model → DocxExporter
 */
namespace DocxIO {

[[nodiscard]] bool load(QTextDocument *document, const QString &filePath,
                        QString *errorMessage = nullptr,
                        DocxDocumentMeta *meta = nullptr);
[[nodiscard]] bool save(QTextDocument *document, const QString &filePath,
                        QString *errorMessage = nullptr,
                        const DocxDocumentMeta *meta = nullptr);

} // namespace DocxIO

#endif // DOCXIO_H
