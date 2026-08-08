#include "headerfootersettings.h"
#include "pagededitorwidget.h"
#include "pagelayout.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QElapsedTimer>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextBlock>
#include <QTextDocument>

#include <cstdio>

namespace {

QString makeDocument(int chars)
{
    const QString paragraph = QStringLiteral(
        "这是一段用于性能基准测试的中文文本，包含 English mix 和标点符号，"
        "用于模拟真实文档的编辑与分页负载。");
    QString out;
    out.reserve(chars + chars / paragraph.size());
    while (out.size() < chars) {
        out += paragraph;
        out += QLatin1Char('\n');
    }
    return out;
}

QString makeHtml(const QString &text)
{
    QString html;
    const QStringList lines = text.split(QLatin1Char('\n'));
    html.reserve(text.size() + lines.size() * 8);
    for (const QString &line : lines)
        html += QStringLiteral("<p>%1</p>").arg(line.toHtmlEscaped());
    return html;
}

void bench(int chars)
{
    const QString text = makeDocument(chars);
    const int kRounds = 30;

    // --- Fragment-structure comparison (why setHtml docs edit slowly) ---
    QTextDocument plainFrag;
    plainFrag.setPlainText(text);
    QTextDocument htmlFrag;
    htmlFrag.setHtml(makeHtml(text));
    QTextDocument normalizedFrag;
    normalizedFrag.setHtml(htmlFrag.toHtml());
    PagedEditorWidget::normalizeDocumentStructure(&normalizedFrag);
    auto countFragments = [](const QTextDocument &d) {
        int n = 0;
        for (QTextBlock b = d.begin(); b.isValid(); b = b.next()) {
            for (auto it = b.begin(); !it.atEnd(); ++it)
                ++n;
        }
        return n;
    };
    std::printf("probe chars=%-8d fragments: plain=%d html=%d normalized=%d blocks=%d\n",
                chars, countFragments(plainFrag), countFragments(htmlFrag),
                countFragments(normalizedFrag), plainFrag.blockCount());

    // --- Qt layout-query cost after an edit (pagination diagnosis) ---
    QTextDocument probe;
    probe.setDocumentMargin(0);
    probe.setPageSize(QSizeF(602, 820));
    probe.setHtml(makeHtml(text));
    (void)probe.documentLayout()->documentSize(); // force full layout
    QElapsedTimer qt;
    qt.start();
    (void)probe.documentLayout()->blockBoundingRect(probe.lastBlock());
    const qint64 cachedQueryUs = qt.elapsed() * 1000;
    QTextCursor pc2(&probe);
    pc2.movePosition(QTextCursor::End);
    qt.restart();
    for (int i = 0; i < 30; ++i)
        pc2.insertText(QStringLiteral("字"));
    const qreal laidOutInsertMs = qreal(qt.elapsed()) / 30;
    (void)probe.documentLayout()->blockBoundingRect(probe.lastBlock());
    const qint64 afterEditQueryMs = qt.elapsed();

    // --- Normalization probe: rebuild the same content via cursor API ---
    QTextDocument norm;
    norm.setDocumentMargin(0);
    norm.setPageSize(QSizeF(602, 820));
    norm.setHtml(makeHtml(text));
    QTextCursor nc(&norm);
    nc.select(QTextCursor::Document);
    const QTextDocumentFragment frag(nc);
    nc.beginEditBlock();
    nc.removeSelectedText();
    nc.insertFragment(frag);
    nc.endEditBlock();
    QTextCursor ncur(&norm);
    ncur.movePosition(QTextCursor::End);
    qt.restart();
    for (int i = 0; i < 30; ++i)
        ncur.insertText(QStringLiteral("字"));
    const qreal normInsertMs = qreal(qt.elapsed()) / 30;
    (void)norm.documentLayout()->documentSize(); // force full layout again
    QTextCursor ncur2(&norm);
    ncur2.movePosition(QTextCursor::End);
    qt.restart();
    for (int i = 0; i < 30; ++i)
        ncur2.insertText(QStringLiteral("字"));
    const qreal normInsertAfterLayoutMs = qreal(qt.elapsed()) / 30;

    // --- Plain-text document, TRULY fully laid out (touch every block) ---
    QTextDocument plain;
    plain.setDocumentMargin(0);
    plain.setPageSize(QSizeF(602, 820));
    plain.setPlainText(text);
    (void)plain.documentLayout()->documentSize();
    for (QTextBlock b = plain.begin(); b.isValid(); b = b.next())
        (void)plain.documentLayout()->blockBoundingRect(b);
    QTextCursor pcur(&plain);
    pcur.movePosition(QTextCursor::End);
    qt.restart();
    for (int i = 0; i < 30; ++i)
        pcur.insertText(QStringLiteral("字"));
    const qreal plainLaidOutInsertMs = qreal(qt.elapsed()) / 30;
    std::printf("probe chars=%-8d cachedQuery=%-4lldus laidOutInsert=%.2fms "
                "afterEditQuery=%lldms normInsert=%.2fms normAfterLayout=%.2fms "
                "plainLaidOut=%.2fms\n",
                chars, cachedQueryUs, laidOutInsertMs, afterEditQueryMs, normInsertMs,
                normInsertAfterLayoutMs, plainLaidOutInsertMs);

    // --- Raw QTextDocument insert (no widget, no pagination) ---
    QTextDocument raw;
    raw.setPlainText(text);
    QTextCursor rawCursor(&raw);
    rawCursor.movePosition(QTextCursor::End);
    QElapsedTimer rt;
    rt.start();
    for (int i = 0; i < kRounds; ++i)
        rawCursor.insertText(QStringLiteral("字"));
    const qreal rawKeyMs = qreal(rt.elapsed()) / kRounds;

    // --- Paginated QTextDocument insert (pageSize set, no widget machinery) ---
    QTextDocument pag;
    pag.setDocumentMargin(0);
    pag.setPageSize(QSizeF(602, 820));
    pag.setPlainText(text);
    QTextCursor pc(&pag);
    pc.movePosition(QTextCursor::End);
    rt.restart();
    for (int i = 0; i < kRounds; ++i)
        pc.insertText(QStringLiteral("字"));
    const qreal paginatedRawMs = qreal(rt.elapsed()) / kRounds;

    // --- Open (setHtml + first pagination) ---
    QTextDocument doc;
    PagedEditorWidget view(&doc, PageLayoutSettings{}, HeaderFooterSettings{});
    QElapsedTimer t;
    t.start();
    // Realistic document: one paragraph per line, not a single giant paragraph.
    QString html;
    const QStringList lines = text.split(QLatin1Char('\n'));
    html.reserve(text.size() + lines.size() * 8);
    for (const QString &line : lines)
        html += QStringLiteral("<p>%1</p>").arg(line.toHtmlEscaped());
    view.setHtml(html);
    const qint64 openMs = t.elapsed();
    const int pages = view.pageCount();

    // --- Per-keystroke append (cursor at end) ---
    QTextCursor endCursor(&doc);
    endCursor.movePosition(QTextCursor::End);
    view.setTextCursor(endCursor);
    t.restart();
    for (int i = 0; i < kRounds; ++i) {
        QTextCursor c = view.textCursor();
        c.insertText(QStringLiteral("字"));
    }
    const qreal perKeyMs = qreal(t.elapsed()) / kRounds;
    if (qEnvironmentVariableIsSet("NEWWORD_BENCH_DEBUG")) {
        // Per-step breakdown for the first few keystrokes.
        QTextCursor probeCursor(&doc);
        probeCursor.movePosition(QTextCursor::End);
        view.setTextCursor(probeCursor);
        for (int i = 0; i < 3; ++i) {
            QElapsedTimer kt;
            kt.start();
            QTextCursor c = view.textCursor();
            const qint64 t1 = kt.elapsed();
            c.insertText(QStringLiteral("字"));
            const qint64 t2 = kt.elapsed();
            std::printf("  key %d: cursor=%lldms insert=%lldms\n", i, t1, t2 - t1);
        }
    }

    // --- Per-keystroke in the middle (worst case) ---
    QTextCursor midCursor(&doc);
    midCursor.setPosition(doc.characterCount() / 2);
    view.setTextCursor(midCursor);
    t.restart();
    for (int i = 0; i < kRounds; ++i) {
        QTextCursor c = view.textCursor();
        c.insertText(QStringLiteral("字"));
    }
    const qreal perKeyMidMs = qreal(t.elapsed()) / kRounds;

    // --- Same document with every widget handler disconnected ---
    QObject::disconnect(&doc, nullptr, &view, nullptr);
    QTextCursor detachedCursor(&doc);
    detachedCursor.movePosition(QTextCursor::End);
    t.restart();
    for (int i = 0; i < kRounds; ++i)
        detachedCursor.insertText(QStringLiteral("字"));
    const qreal detachedMs = qreal(t.elapsed()) / kRounds;

    // --- Pagination rebuild cost (the per-edit full pass) ---
    t.restart();
    view.setTextCursor(QTextCursor(&doc));
    const qint64 pageRebuildMs = t.elapsed();

    std::printf("chars=%-8d open=%-6lldms pages=%-4d appendKey=%.2fms middleKey=%.2fms "
                "cursorMove=%-3lldms rawInsert=%.2fms paginatedRaw=%.2fms detached=%.2fms\n",
                chars, openMs, pages, perKeyMs, perKeyMidMs, pageRebuildMs, rawKeyMs,
                paginatedRawMs, detachedMs);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    std::printf("--- PagedEditorWidget performance (offscreen) ---\n");
    bench(10'000);
    bench(50'000);
    bench(200'000);
    bench(800'000);
    return 0;
}
