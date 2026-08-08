#ifndef ENGINE_DOCXIMPORTER_H
#define ENGINE_DOCXIMPORTER_H

#include "documentmodel.h"
#include "docxmeta.h"

#include <QString>

namespace Engine {

/**
 * Phase 2 closed loop: DOCX → DocumentModel (no QTextDocument).
 * Mirrors DocxExporter coverage: paragraphs, char styles, headings,
 * page breaks, inline images, simple tables; optional meta via DocxPackage.
 */
namespace DocxImporter {

[[nodiscard]] bool load(DocumentModel *model,
                        const QString &filePath,
                        QString *errorMessage = nullptr,
                        DocxDocumentMeta *meta = nullptr);

} // namespace DocxImporter

} // namespace Engine

#endif // ENGINE_DOCXIMPORTER_H
