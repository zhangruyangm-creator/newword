#ifndef ENGINE_DOCXEXPORTER_H
#define ENGINE_DOCXEXPORTER_H

#include "documentmodel.h"
#include "docxmeta.h"

#include <QString>

namespace Engine {

/**
 * Phase 2: DocumentModel → DOCX (no HTML / no QTextDocument walk).
 * Covers paragraphs, char styles, headings, page breaks, images, tables,
 * plus optional page setup / plain headers & footers.
 */
namespace DocxExporter {

[[nodiscard]] bool save(const DocumentModel &model,
                        const QString &filePath,
                        QString *errorMessage = nullptr,
                        const DocxDocumentMeta *meta = nullptr);

} // namespace DocxExporter

} // namespace Engine

#endif // ENGINE_DOCXEXPORTER_H
