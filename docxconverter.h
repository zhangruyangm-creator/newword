#ifndef DOCXCONVERTER_H
#define DOCXCONVERTER_H

#include "docxmeta.h"

#include <QString>
#include <atomic>

class QTextDocument;

/**
 * DOCX 主路径（v0.5 / 阶段 2）：
 *   导入：优先 Python 桥接（mammoth）；失败则内置 DocxIO::load。
 *   导出：优先桥接（html2docx）；失败则 QTextDocument → DocumentModel → DocxExporter。
 *   例外：含脚注/尾注/批注时强制走内置 Model 路径（桥接 HTML 不保留审阅部件）。
 *   页眉页脚 / 纸张：经 DocxPackage 写入或读取（与正文路径叠加）。
 * 不承诺 Word 级保真。
 */
namespace DocxConverter {

enum class Backend {
    None,
    Bridge,
    Builtin
};

//! Result of an off-GUI-thread DOCX→HTML preparation (bridge path).
struct PrepareResult {
    bool ok = false;
    //! Bridge unavailable/failed — caller should run DocxIO::load on the GUI thread.
    bool useBuiltinOnGui = false;
    bool cancelled = false;
    QString error;
    QString html;
    DocxDocumentMeta meta;
    Backend backend = Backend::None;
    QString statusNote;
};

[[nodiscard]] bool bridgeAvailable();
//! Non-blocking bridge check for GUI-thread callers (kicks off an async refresh
//! when stale; never runs a subprocess synchronously).
[[nodiscard]] bool bridgeReady();
//! Background warm-up of the bridge availability check (safe at startup).
void warmUpBridge();
[[nodiscard]] QString bridgeStatusText();
[[nodiscard]] QString primaryPathDescription();
[[nodiscard]] Backend lastBackend();
//! Non-fatal note after last load/save (e.g. fell back to builtin). Empty if none.
[[nodiscard]] QString lastStatusNote();

//! Heavy work safe off the GUI thread: mammoth → HTML + meta. Does not touch QTextDocument.
[[nodiscard]] PrepareResult prepareImport(const QString &filePath,
                                          std::atomic_bool *cancelled = nullptr);

//! Apply bridge HTML (or run builtin load) onto a document — GUI thread only.
[[nodiscard]] bool applyPrepared(QTextDocument *document,
                                 const PrepareResult &prepared,
                                 const QString &filePath,
                                 QString *errorMessage = nullptr,
                                 DocxDocumentMeta *metaOut = nullptr);

[[nodiscard]] bool load(QTextDocument *document, const QString &filePath,
                        QString *errorMessage = nullptr,
                        DocxDocumentMeta *meta = nullptr);
[[nodiscard]] bool save(QTextDocument *document, const QString &filePath,
                        QString *errorMessage = nullptr,
                        const DocxDocumentMeta *meta = nullptr);

} // namespace DocxConverter

#endif // DOCXCONVERTER_H
