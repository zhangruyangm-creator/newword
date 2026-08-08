//! Micro-benchmark: live pagination cost before vs after the Fast/async Precise split.
//!
//! "Before" ≈ every Precise update blocked the GUI on Adapter + LayoutEngine.
//! "After"  ≈ Fast is geometric-only; Precise keeps Adapter on GUI and LayoutEngine off-thread.
//!
//! Build: cmake --build --preset qt6-macos-debug --target NewWordBenchLayoutPerf
//! Run:   ./build/NewWordBenchLayoutPerf

#include "layoutengine.h"
#include "pagegeometry.h"
#include "pagelayout.h"
#include "qtextadapter.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace {

struct Sample {
    const char *name = nullptr;
    qint64 ns = 0;
    int iters = 0;

    [[nodiscard]] double usPerCall() const
    {
        return iters > 0 ? (double(ns) / double(iters) / 1000.0) : 0.0;
    }
};

template <typename Fn>
Sample bench(const char *name, int iters, Fn &&fn)
{
    for (int i = 0; i < qMin(3, iters); ++i)
        fn();

    QElapsedTimer t;
    t.start();
    for (int i = 0; i < iters; ++i)
        fn();
    Sample s;
    s.name = name;
    s.ns = t.nsecsElapsed();
    s.iters = iters;
    return s;
}

std::unique_ptr<QTextDocument> makeDoc(int paragraphs, bool withTable)
{
    auto doc = std::make_unique<QTextDocument>();
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);

    const PageLayoutSettings layout;
    const PageGeometry geo = PageGeometry::from(layout, 100);
    doc->setDocumentMargin(geo.marginPx);
    doc->setPageSize(QSizeF(geo.pageWidthPx, -1));

    QTextCursor c(doc.get());
    for (int i = 0; i < paragraphs; ++i) {
        if (i > 0)
            c.insertBlock();
        c.insertText(QStringLiteral(
                         "Bench paragraph %1. The quick brown fox jumps over the lazy dog. "
                         "中文对照：分页性能基准，含中英混排与标点，。！？")
                         .arg(i + 1));
    }

    if (withTable) {
        c.insertBlock();
        QTextTable *table = c.insertTable(12, 3);
        for (int r = 0; r < 12; ++r) {
            for (int col = 0; col < 3; ++col) {
                table->cellAt(r, col).firstCursorPosition().insertText(
                    QStringLiteral("R%1C%2 单元格").arg(r + 1).arg(col + 1));
            }
        }
    }

    (void)doc->size();
    return doc;
}

void printRow(const char *label, double us)
{
    if (us < 1.0)
        std::printf("  %-28s %10.3f µs\n", label, us);
    else
        std::printf("  %-28s %10.1f µs\n", label, us);
}

void runCase(const char *title, int paragraphs, bool withTable, int iters)
{
    auto doc = makeDoc(paragraphs, withTable);
    const PageLayoutSettings layout;
    const PageGeometry geo = PageGeometry::from(layout, 100);
    const int onePageBody = PageGeometry::contentBodyHeightPx(layout, 100);

    // Warm Qt text layout once.
    (void)doc->size().height();

    const Sample oldFull = bench("old_full", iters, [&]() {
        const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(doc.get(), layout);
        (void)Engine::LayoutEngine::layout(model, layout);
    });

    const Sample adapterOnly = bench("adapter", iters, [&]() {
        (void)Engine::QTextAdapter::fromDocument(doc.get(), layout);
    });

    const Engine::DocumentModel warmModel = Engine::QTextAdapter::fromDocument(doc.get(), layout);
    const Sample enginePure = bench("engine_pure", iters, [&]() {
        (void)Engine::LayoutEngine::layout(warmModel, layout);
    });

    const Sample fast = bench("fast", qMax(iters * 50, 2000), [&]() {
        const int docHeight = qMax(1, qRound(doc->size().height()) + 24);
        const int pages = qMax(1, (docHeight + onePageBody - 1) / onePageBody);
        volatile int sink = pages * onePageBody + geo.pageWidthPx;
        (void)sink;
    });

    const int pages = Engine::LayoutEngine::pageCount(warmModel, layout);

    std::printf("\n=== %s (%d paragraphs%s, ~%d pages, heavy iters=%d) ===\n",
                title, paragraphs, withTable ? " + table" : "", pages, iters);
    printRow("Before: Adapter+Engine (UI)", oldFull.usPerCall());
    printRow("After:  Fast path (UI)", fast.usPerCall());
    printRow("After:  Adapter only (UI)", adapterOnly.usPerCall());
    printRow("After:  Engine (background)", enginePure.usPerCall());

    const double uiBefore = oldFull.usPerCall();
    const double uiAfterPause = adapterOnly.usPerCall() + fast.usPerCall();
    const double uiDuringTyping = qMax(fast.usPerCall(), 0.001);

    std::printf("  %-28s %10.1fx\n", "UI speedup @ typing pause",
                uiBefore > 0 ? uiBefore / uiAfterPause : 0.0);
    std::printf("  %-28s %10.1fx\n", "UI speedup @ Fast-only tick",
                uiBefore > 0 ? uiBefore / uiDuringTyping : 0.0);
    printRow("UI work removed (Engine)", enginePure.usPerCall());
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    std::printf("NewWord live-pagination micro-benchmark\n");
    std::printf("Machine-local; compare relative ratios, not absolute µs across machines.\n");

    runCase("Short", 8, false, 80);
    runCase("Medium", 60, false, 40);
    runCase("Long", 180, false, 20);
    runCase("Long+table", 120, true, 15);

    // Burst model: 80 keystrokes, 15ms apart (~1.2s typing), then one settle.
    // Before: debounce 50ms → 1× full Adapter+Engine on UI.
    // After:  debounce 70/200ms → 1× Fast + 1× Adapter on UI; Engine async.
    std::printf("\n=== Simulated pause-after-typing (1 settle event) ===\n");
    {
        auto doc = makeDoc(120, true);
        const PageLayoutSettings layout;
        const int iters = 25;

        const Sample before = bench("settle_before", iters, [&]() {
            const auto model = Engine::QTextAdapter::fromDocument(doc.get(), layout);
            (void)Engine::LayoutEngine::layout(model, layout);
        });
        const Sample afterUi = bench("settle_after_ui", iters, [&]() {
            (void)Engine::QTextAdapter::fromDocument(doc.get(), layout);
            const int docHeight = qMax(1, qRound(doc->size().height()) + 24);
            (void)docHeight;
        });

        std::printf("  Before UI block:  %8.1f µs\n", before.usPerCall());
        std::printf("  After  UI block:  %8.1f µs  (Engine off-thread)\n", afterUi.usPerCall());
        std::printf("  UI latency cut:   %8.1fx\n",
                    afterUi.usPerCall() > 0 ? before.usPerCall() / afterUi.usPerCall() : 0.0);
    }

    std::printf("\n=== Incremental SnapshotCache (type at end of long doc) ===\n");
    {
        auto doc = makeDoc(180, false);
        const PageLayoutSettings layout;
        Engine::QTextAdapter::SnapshotCache cache;
        (void)cache.ensure(doc.get(), layout);

        QTextCursor c(doc.get());
        c.movePosition(QTextCursor::End);
        double fullUs = 0;
        double incrUs = 0;
        const int iters = 40;
        for (int i = 0; i < iters; ++i) {
            const int pos = c.position();
            c.insertText(QStringLiteral("y"));
            {
                QElapsedTimer t;
                t.start();
                (void)Engine::QTextAdapter::fromDocument(doc.get(), layout);
                fullUs += t.nsecsElapsed() / 1000.0;
            }
            {
                cache.noteChange(pos);
                QElapsedTimer t;
                t.start();
                (void)cache.ensure(doc.get(), layout);
                incrUs += t.nsecsElapsed() / 1000.0;
            }
        }
        std::printf("  Full adapter / key:          %8.1f µs\n", fullUs / iters);
        std::printf("  Incremental adapter / key:   %8.1f µs\n", incrUs / iters);
        std::printf("  Adapter speedup (end typing): %8.1fx\n",
                    incrUs > 0 ? fullUs / incrUs : 0.0);
    }

    std::printf("\nDone.\n");
    return 0;
}
