#include "docxio.h"
#include "docxexporter.h"
#include "docxpackage.h"
#include "qtextadapter.h"

#include <QTextDocument>

bool DocxIO::save(QTextDocument *document, const QString &filePath, QString *errorMessage,
                  const DocxDocumentMeta *meta)
{
    if (!document) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效文档。");
        return false;
    }
    PageLayoutSettings layout;
    if (meta && meta->writePageLayout)
        layout = meta->pageLayout;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(document, layout);
    return Engine::DocxExporter::save(model, filePath, errorMessage, meta);
}
