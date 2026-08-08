#include "docxio.h"
#include "docximporter.h"
#include "docxpackage.h"
#include "qtextadapter.h"

#include <QTextDocument>

bool DocxIO::load(QTextDocument *document, const QString &filePath, QString *errorMessage,
                  DocxDocumentMeta *meta)
{
    if (!document) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效文档。");
        return false;
    }

    Engine::DocumentModel model;
    DocxDocumentMeta localMeta;
    DocxDocumentMeta *metaPtr = meta ? meta : &localMeta;
    if (!Engine::DocxImporter::load(&model, filePath, errorMessage, metaPtr))
        return false;

    Engine::QTextAdapter::toDocument(model, document);
    return true;
}
