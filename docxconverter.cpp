#include "docxconverter.h"
#include "docxio.h"
#include "docxpackage.h"
#include "formulaio.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrent>

#include <atomic>

namespace DocxConverter {
namespace {

Backend g_lastBackend = Backend::None;
QString g_lastStatusNote;

bool runBridge(const QStringList &args, QString *errorMessage, int timeoutMs,
               std::atomic_bool *cancelled);

// Thread-safe, time-bounded cache of the bridge availability check. A plain
// `static int` cached the first result forever and blocked the GUI thread for up
// to 15s on the first call; the cache now expires so a later `setup.sh` takes
// effect without restarting the app, and GUI callers use bridgeReady(), which
// never runs a subprocess synchronously.
std::atomic<int> g_bridgeCache{-1};         // -1 unknown, 0 unavailable, 1 available
std::atomic<qint64> g_bridgeCheckedAtMs{0}; // 0 = never checked
std::atomic<bool> g_bridgeChecking{false};

constexpr qint64 kBridgeCheckRetryMs = 30000;

bool bridgeCacheFresh()
{
    const qint64 at = g_bridgeCheckedAtMs.load(std::memory_order_relaxed);
    return at > 0 && (QDateTime::currentMSecsSinceEpoch() - at) < kBridgeCheckRetryMs;
}

// Runs the python check synchronously on the calling thread. Callers on the GUI
// thread must not use this path directly (see bridgeReady()).
bool runBridgeCheck()
{
    QString error;
    const bool ok = runBridge({QStringLiteral("check")}, &error, 15000, nullptr);
    g_bridgeCache.store(ok ? 1 : 0, std::memory_order_relaxed);
    g_bridgeCheckedAtMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
    g_bridgeChecking.store(false, std::memory_order_release);
    return ok;
}

void scheduleBridgeCheck()
{
    bool expected = false;
    if (!g_bridgeChecking.compare_exchange_strong(expected, true))
        return; // a check is already in flight
    (void)QtConcurrent::run([]() { (void)runBridgeCheck(); });
}

bool documentHasFootnotes(const QTextDocument *document)
{
    if (!document)
        return false;
    return !document
                ->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://footnotes")))
                .toMap()
                .isEmpty();
}

bool documentHasComments(const QTextDocument *document)
{
    if (!document)
        return false;
    return !document
                ->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://comments")))
                .toMap()
                .isEmpty();
}

bool documentHasEndnotes(const QTextDocument *document)
{
    if (!document)
        return false;
    return !document
                ->resource(QTextDocument::UserResource, QUrl(QStringLiteral("newword://endnotes")))
                .toMap()
                .isEmpty();
}

bool documentNeedsBuiltinDocx(const QTextDocument *document)
{
    return documentHasFootnotes(document) || documentHasEndnotes(document)
        || documentHasComments(document);
}

bool packageNeedsBuiltinDocx(const QString &filePath)
{
    return DocxPackage::hasFootnotes(filePath) || DocxPackage::hasEndnotes(filePath)
        || DocxPackage::hasComments(filePath);
}

QString bridgeScriptPath()
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../Resources/docx_bridge/docx_bridge.py")),
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("docx_bridge/docx_bridge.py")),
        QStringLiteral(NEWWORD_SOURCE_DIR "/tools/docx_bridge/docx_bridge.py"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QString bridgePythonPath()
{
    if (const QByteArray env = qgetenv("NEWWORD_DOCX_PYTHON"); !env.isEmpty()) {
        const QString path = QString::fromLocal8Bit(env);
        if (QFileInfo::exists(path))
            return path;
    }

    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../Resources/docx_venv/bin/python3")),
        QStringLiteral(NEWWORD_SOURCE_DIR "/.venv/bin/python3"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .absoluteFilePath(QStringLiteral("docx_venv/bin/python3")),
        QStringLiteral("/usr/bin/python3"),
        QStringLiteral("python3"),
    };
    for (const QString &path : candidates) {
        if (path == QLatin1String("python3"))
            return path;
        if (QFileInfo::exists(path))
            return path;
    }
    return QStringLiteral("python3");
}

bool runBridge(const QStringList &args, QString *errorMessage, int timeoutMs = 120000,
               std::atomic_bool *cancelled = nullptr)
{
    const QString script = bridgeScriptPath();
    if (script.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "未找到 DOCX 桥接脚本（docx_bridge.py）。请确认应用已正确打包，"
                "或从源码运行 tools/docx_bridge/setup.sh。");
        return false;
    }

    QProcess proc;
    QStringList fullArgs = {script};
    fullArgs.append(args);
    proc.start(bridgePythonPath(), fullArgs);
    if (!proc.waitForStarted(5000)) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "无法启动 DOCX 桥接 Python（%1）。请运行 tools/docx_bridge/setup.sh 创建 .venv。")
                                .arg(bridgePythonPath());
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (!proc.waitForFinished(200)) {
        if (cancelled && cancelled->load()) {
            proc.kill();
            proc.waitForFinished(3000);
            if (errorMessage)
                *errorMessage = QStringLiteral("已取消。");
            return false;
        }
        if (timer.elapsed() > timeoutMs) {
            proc.kill();
            proc.waitForFinished(3000);
            if (errorMessage)
                *errorMessage = QStringLiteral("DOCX 桥接超时（文档可能过大或 Python 卡住）。");
            return false;
        }
    }
    if (cancelled && cancelled->load()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("已取消。");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (errorMessage) {
            *errorMessage = err.isEmpty()
                ? QStringLiteral("DOCX 桥接失败（exit %1）。").arg(proc.exitCode())
                : err;
        }
        return false;
    }
    return true;
}

bool loadBuiltin(QTextDocument *document, const QString &filePath, QString *errorMessage,
                 DocxDocumentMeta *meta, const QString &bridgeError, bool bridgeWasTried)
{
    QString builtinError;
    if (!DocxIO::load(document, filePath, &builtinError, meta)) {
        if (errorMessage) {
            if (!bridgeError.isEmpty() && bridgeWasTried) {
                *errorMessage = QStringLiteral(
                    "无法打开 DOCX。\n"
                    "增强引擎失败：%1\n"
                    "内置引擎失败：%2")
                                    .arg(bridgeError, builtinError);
            } else if (!bridgeWasTried) {
                *errorMessage = QStringLiteral(
                    "无法打开 DOCX（%1）。\n"
                    "提示：当前仅使用内置子集；运行 tools/docx_bridge/setup.sh 可启用增强导入。")
                                    .arg(builtinError);
            } else {
                *errorMessage = builtinError;
            }
        }
        return false;
    }

    g_lastBackend = Backend::Builtin;
    if (bridgeWasTried) {
        g_lastStatusNote = QStringLiteral(
            "增强引擎失败，已用内置引擎打开（支持表格合并/标题样式等；"
            "复杂版式、文本框、SmartArt 等可能丢失）");
    } else {
        g_lastStatusNote = QStringLiteral(
            "已用内置引擎打开 DOCX（支持表格合并/标题样式等；"
            "复杂版式、文本框、SmartArt 等可能丢失）");
    }
    return true;
}

} // namespace

bool bridgeAvailable()
{
    if (bridgeCacheFresh())
        return g_bridgeCache.load(std::memory_order_relaxed) == 1;

    // Stale or never checked: run the check now (blocking). If another thread
    // already has a check in flight, reuse the last known answer instead of
    // stacking a second subprocess.
    bool expected = false;
    if (!g_bridgeChecking.compare_exchange_strong(expected, true))
        return g_bridgeCache.load(std::memory_order_relaxed) == 1;
    return runBridgeCheck();
}

//! Non-blocking variant for GUI-thread callers: never runs a subprocess
//! synchronously; kicks off an async refresh when the cache is stale.
bool bridgeReady()
{
    if (!bridgeCacheFresh())
        scheduleBridgeCheck();
    return g_bridgeCache.load(std::memory_order_relaxed) == 1;
}

//! Background warm-up so the first save/About does not wait on a check.
void warmUpBridge()
{
    if (!bridgeCacheFresh())
        scheduleBridgeCheck();
}

QString bridgeStatusText()
{
    if (bridgeReady())
        return QStringLiteral("增强（mammoth / html2docx）");
    return QStringLiteral("内置子集（运行 tools/docx_bridge/setup.sh 可启用增强）");
}

QString primaryPathDescription()
{
    return QStringLiteral(
        "DOCX：导入优先 Python 桥接（mammoth）；导出优先桥接（html2docx），"
        "回退为 DocumentModel → DocxExporter。"
        "不承诺与 Microsoft Word 完全一致。");
}

Backend lastBackend()
{
    return g_lastBackend;
}

QString lastStatusNote()
{
    return g_lastStatusNote;
}

PrepareResult prepareImport(const QString &filePath, std::atomic_bool *cancelled)
{
    PrepareResult result;
    if (cancelled && cancelled->load()) {
        result.cancelled = true;
        result.error = QStringLiteral("已取消。");
        return result;
    }

    if (!bridgeAvailable()) {
        result.useBuiltinOnGui = true;
        result.error = QStringLiteral("增强引擎未就绪");
        return result;
    }

    // mammoth HTML drops OOXML footnotes/comments — keep Model/DocxIO path for fidelity.
    if (packageNeedsBuiltinDocx(filePath)) {
        result.useBuiltinOnGui = true;
        result.statusNote = QStringLiteral(
            "文档含脚注/尾注/批注，已用内置引擎打开（保留审阅部件；"
            "复杂版式、文本框等仍可能丢失）");
        return result;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        result.error = QStringLiteral("无法创建临时目录。");
        result.useBuiltinOnGui = true;
        return result;
    }

    const QString htmlPath = tmp.filePath(QStringLiteral("import.html"));
    QString bridgeError;
    if (!runBridge({QStringLiteral("import"), filePath, htmlPath}, &bridgeError, 120000, cancelled)) {
        if (cancelled && cancelled->load()) {
            result.cancelled = true;
            result.error = QStringLiteral("已取消。");
            return result;
        }
        result.useBuiltinOnGui = true;
        result.error = bridgeError;
        return result;
    }

    if (cancelled && cancelled->load()) {
        result.cancelled = true;
        result.error = QStringLiteral("已取消。");
        return result;
    }

    QFile htmlFile(htmlPath);
    if (!htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.useBuiltinOnGui = true;
        result.error = QStringLiteral("桥接已生成 HTML，但无法读取临时文件。");
        return result;
    }

    result.html = QString::fromUtf8(htmlFile.readAll());
    htmlFile.close();
    (void)DocxPackage::readMeta(filePath, &result.meta);
    result.ok = true;
    result.backend = Backend::Bridge;
    result.statusNote = QStringLiteral(
        "已用增强引擎打开 DOCX（mammoth HTML；部分 OOXML 版式/域可能简化）");
    return result;
}

bool applyPrepared(QTextDocument *document, const PrepareResult &prepared, const QString &filePath,
                   QString *errorMessage, DocxDocumentMeta *metaOut)
{
    g_lastStatusNote.clear();
    g_lastBackend = Backend::None;

    if (!document) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效文档。");
        return false;
    }

    if (prepared.cancelled) {
        if (errorMessage)
            *errorMessage = prepared.error.isEmpty() ? QStringLiteral("已取消。") : prepared.error;
        return false;
    }

    if (prepared.ok && !prepared.html.isEmpty()) {
        document->setHtml(prepared.html);
        FormulaIO::restoreFormulasFromHtml(document);
        document->setModified(false);
        g_lastBackend = prepared.backend;
        g_lastStatusNote = prepared.statusNote;
        if (metaOut)
            *metaOut = prepared.meta;
        return true;
    }

    if (prepared.useBuiltinOnGui) {
        DocxDocumentMeta meta;
        const bool bridgeTried = bridgeReady() && prepared.statusNote.isEmpty();
        if (!loadBuiltin(document, filePath, errorMessage, &meta, prepared.error, bridgeTried))
            return false;
        if (!prepared.statusNote.isEmpty()) {
            g_lastBackend = Backend::Builtin;
            g_lastStatusNote = prepared.statusNote;
        }
        if (metaOut)
            *metaOut = meta;
        return true;
    }

    if (errorMessage)
        *errorMessage = prepared.error.isEmpty() ? QStringLiteral("无法打开 DOCX。") : prepared.error;
    return false;
}

bool load(QTextDocument *document, const QString &filePath, QString *errorMessage,
          DocxDocumentMeta *meta)
{
    const PrepareResult prepared = prepareImport(filePath, nullptr);
    return applyPrepared(document, prepared, filePath, errorMessage, meta);
}

bool save(QTextDocument *document, const QString &filePath, QString *errorMessage,
          const DocxDocumentMeta *meta)
{
    g_lastStatusNote.clear();
    g_lastBackend = Backend::None;

    if (!document) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无效文档。");
        return false;
    }

    QString bridgeError;
    // html2docx cannot emit footnotes/comments — keep Model → DocxExporter for fidelity.
    if (bridgeReady() && !documentNeedsBuiltinDocx(document)) {
        QTemporaryDir tmp;
        if (!tmp.isValid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法创建临时目录。");
            return false;
        }
        const QString htmlPath = tmp.filePath(QStringLiteral("export.html"));
        QFile htmlFile(htmlPath);
        if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = htmlFile.errorString();
            return false;
        }
        const QString html = FormulaIO::documentToHtmlWithFormulas(document);
        const QByteArray htmlBytes = html.toUtf8();
        if (htmlFile.write(htmlBytes) != htmlBytes.size()) {
            if (errorMessage)
                *errorMessage = htmlFile.errorString().isEmpty()
                    ? QStringLiteral("写入临时 HTML 失败")
                    : htmlFile.errorString();
            return false;
        }
        htmlFile.close();

        const QString title = QFileInfo(filePath).completeBaseName();
        if (runBridge({QStringLiteral("export"), htmlPath, filePath,
                       QStringLiteral("--title"), title},
                      &bridgeError)) {
            g_lastBackend = Backend::Bridge;
            g_lastStatusNote = QStringLiteral(
                "已用增强引擎保存 DOCX（html2docx；复杂版式可能简化）");
            if (meta) {
                QString patchErr;
                if (!DocxPackage::applyMeta(filePath, *meta, &patchErr)) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("DOCX 正文已导出，但页眉页脚/纸张写入失败：%1")
                                            .arg(patchErr);
                    g_lastStatusNote += QStringLiteral("；页眉页脚写入失败：") + patchErr;
                    return false;
                }
                g_lastStatusNote += QStringLiteral("（已写入页眉页脚/纸张）");
            }
            return true;
        }
    } else {
        bridgeError = QStringLiteral("增强引擎未就绪");
    }

    QString builtinError;
    if (!DocxIO::save(document, filePath, &builtinError, meta)) {
        if (errorMessage) {
            if (!bridgeError.isEmpty() && bridgeReady()) {
                *errorMessage = QStringLiteral(
                    "无法保存 DOCX。\n"
                    "增强引擎失败：%1\n"
                    "内置引擎失败：%2")
                                    .arg(bridgeError, builtinError);
            } else if (!bridgeReady()) {
                *errorMessage = QStringLiteral(
                    "无法保存 DOCX（%1）。\n"
                    "提示：运行 tools/docx_bridge/setup.sh 可启用增强导出。")
                                    .arg(builtinError);
            } else {
                *errorMessage = builtinError;
            }
        }
        return false;
    }

    g_lastBackend = Backend::Builtin;
    if (documentNeedsBuiltinDocx(document) && bridgeReady()) {
        g_lastStatusNote = QStringLiteral(
            "文档含脚注/尾注/批注，已用内置引擎保存（保留审阅部件与表格合并；"
            "复杂版式可能简化）");
    } else if (bridgeAvailable()) {
        g_lastStatusNote = QStringLiteral(
            "增强引擎失败，已用内置引擎保存（表格合并/标题等可保留；"
            "复杂版式、SmartArt 等可能丢失）");
    } else {
        g_lastStatusNote = QStringLiteral(
            "已用内置引擎保存 DOCX（表格合并/标题等；复杂版式可能简化）");
    }
    return true;
}

} // namespace DocxConverter
