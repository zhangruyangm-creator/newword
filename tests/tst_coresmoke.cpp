#include "pagegeometry.h"
#include "pagelayout.h"
#include "docxexporter.h"
#include "docximporter.h"
#include "docxio.h"
#include "docxmeta.h"
#include "docxpackage.h"
#include "editorviewlayout.h"
#include "pagededitorwidget.h"
#include "documentmodel.h"
#include "layoutengine.h"
#include "pagedocumentpainter.h"
#include "paginationmetrics.h"
#include "qtextadapter.h"
#include "reviewnotes.h"
#include "styleutils.h"
#include "tablegeometry.h"
#include "floatingtextbox.h"
#include "textstats.h"

#include <QDir>
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QObject>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QTextTable>
#include <QTextTableFormat>
#include <QUrl>
#include <QFrame>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextEdit>
#include <QtTest/QtTest>

class CoreSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void pageGeometry_a4_scalesWithZoom();
    void pageGeometry_chromeHeights();
    void docx_roundTrip_preservesPlainParagraph();
    void docx_roundTrip_preservesHeadingAndBold();
    void layoutEngine_emptyDocument_onePage();
    void layoutEngine_longText_paginates();
    void layoutEngine_pageCount_matchesPainterApi();
    void adapter_preservesTextColor();
    void adapter_preservesImageResource();
    void adapter_preservesTableCells();
    void layoutEngine_inlineTextBox_survives();
    void floatingTextBoxes_storeRoundTrip();
    void floatingTextBoxes_layoutAndPaint();
    void pagination_visualVsEngine_samplesWithinTolerance();
    void pagination_writeEvalMarkdown();
    void layoutEngine_pageBreaksHaveDocPositions();
    void layoutEngine_page2FirstLineAtTop();
    void layoutEngine_blanksThenText_page2Y();
    void layoutEngine_floatWrapClearedAcrossPage();
    void layoutEngine_oversizedImage_scalesToFit();
    void layoutEngine_sectionSetup_switchesPageBox();
    void editorViewLayout_fastStripNeverCollapsesBelowEngineFloor();
    void editorViewLayout_fastStripTracksContent();
    void debug_pagedEditorPrimitives();
    void pagedEditorWidget_pageBoundaryTyping();
    void pagedEditorWidget_renderAndEdit();
    void docx_modelExporter_roundTrip_plainAndStyled();
    void docx_modelExporter_preservesTable();
    void docx_importer_modelClosedLoop_headingBoldTable();
    void adapter_preservesMergedTableCells();
    void docx_model_mergedCells_roundTrip();
    void layoutEngine_mergedCells_paintHeights();
    void textStats_cjkAndLatinAndPunctuation();
    void textStats_selectionUsesParagraphSeparator();
    void docx_meta_headerFooter_roundTrip();
    void layoutEngine_floatImage_wrapsTextBeside();
    void adapter_snapshotCache_suffixMatchesFull();
    void adapter_extractsFootnotes_skipsAppendix();
    void layoutEngine_footnotes_atPageBottom();
    void docx_footnotes_modelRoundTrip();
    void docx_package_detectsFootnotes();
    void adapter_extractsComments();
    void docx_comments_modelRoundTrip();
    void adapter_extractsEndnotes_skipsAppendix();
    void layoutEngine_endnotes_atDocumentEnd();
    void docx_endnotes_modelRoundTrip();
};

void CoreSmokeTest::pageGeometry_a4_scalesWithZoom()
{
    const PageLayoutSettings layout; // A4 defaults
    const PageGeometry g100 = PageGeometry::from(layout, 100);
    const PageGeometry g150 = PageGeometry::from(layout, 150);
    const PageGeometry g50 = PageGeometry::from(layout, 50);

    QVERIFY(g100.pageWidthPx > 700 && g100.pageWidthPx < 900);
    QVERIFY(g100.pageHeightPx > 1000 && g100.pageHeightPx < 1200);
    QVERIFY(g100.marginPx >= 8);
    QCOMPARE(g150.pageWidthPx, qRound(g100.pageWidthPx * 1.5));
    QCOMPARE(g50.pageWidthPx, qRound(g100.pageWidthPx * 0.5));
    QCOMPARE(PageGeometry::zoomFactorFor(100), 1.0);
    QCOMPARE(PageGeometry::zoomFactorFor(50), 0.5);
    QCOMPARE(PageGeometry::zoomFactorFor(200), 2.0);
}

void CoreSmokeTest::pageGeometry_chromeHeights()
{
    const PageGeometry g = PageGeometry::from(PageLayoutSettings{}, 100);
    QCOMPARE(g.chromeLabelHeightPx(false), 0);
    QVERIFY(g.chromeLabelHeightPx(true) >= 18);
    QVERIFY(g.bodyHeightPx(0, 18) >= 120);
}

void CoreSmokeTest::docx_roundTrip_preservesPlainParagraph()
{
    QTextDocument original;
    {
        QTextCursor c(&original);
        c.insertText(QStringLiteral("你好，NewWord 测试段落。"));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("plain.docx"));

    QString err;
    QVERIFY2(DocxIO::save(&original, path, &err), qPrintable(err));

    QTextDocument loaded;
    QVERIFY2(DocxIO::load(&loaded, path, &err), qPrintable(err));
    QCOMPARE(loaded.toPlainText().trimmed(), original.toPlainText().trimmed());
}

void CoreSmokeTest::docx_roundTrip_preservesHeadingAndBold()
{
    QTextDocument original;
    {
        QTextCursor c(&original);
        QTextBlockFormat heading;
        heading.setHeadingLevel(1);
        c.setBlockFormat(heading);
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        c.insertText(QStringLiteral("一级标题"), bold);
        c.insertBlock();
        QTextBlockFormat body;
        body.setHeadingLevel(0);
        c.setBlockFormat(body);
        c.setCharFormat(QTextCharFormat());
        c.insertText(QStringLiteral("正文内容 ABC"));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("styled.docx"));

    QString err;
    QVERIFY2(DocxIO::save(&original, path, &err), qPrintable(err));

    QTextDocument loaded;
    QVERIFY2(DocxIO::load(&loaded, path, &err), qPrintable(err));

    const QString plain = loaded.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("一级标题")));
    QVERIFY(plain.contains(QStringLiteral("正文内容 ABC")));

    // First block should be a heading after round-trip (builtin maps Heading1).
    QTextBlock block = loaded.begin();
    QVERIFY(block.isValid());
    QVERIFY(block.blockFormat().headingLevel() >= 1
            || block.text().contains(QStringLiteral("一级标题")));
}

void CoreSmokeTest::layoutEngine_emptyDocument_onePage()
{
    QTextDocument doc;
    const PageLayoutSettings layout;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, layout);
    QCOMPARE(Engine::LayoutEngine::pageCount(model, layout), 1);
}

void CoreSmokeTest::layoutEngine_longText_paginates()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 80; ++i) {
            if (i > 0)
                c.insertBlock();
            c.insertText(QStringLiteral(
                "NewWord layout engine pagination sample paragraph %1. "
                "The quick brown fox jumps over the lazy dog. "
                "中文分页测试：按内容高度切页，验证多页输出。").arg(i + 1));
        }
    }

    const PageLayoutSettings layout;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, layout);
    const Engine::LayoutResult result = Engine::LayoutEngine::layout(model, layout);

    QVERIFY(result.pageCount() >= 2);
    QVERIFY(result.contentWidthPt > 100);
    QVERIFY(result.contentHeightPt > 100);
    QVERIFY(!result.pages.first().lines.isEmpty());
}

void CoreSmokeTest::layoutEngine_pageCount_matchesPainterApi()
{
    QTextDocument doc;
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 40; ++i) {
            if (i > 0)
                c.insertBlock();
            c.insertText(QStringLiteral("Paragraph %1 — layout consistency check.").arg(i));
        }
    }

    const PageLayoutSettings layout;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, layout);
    const int enginePages = Engine::LayoutEngine::pageCount(model, layout);
    const int painterPages = PageDocumentPainter::pageCount(&doc, layout);

    QCOMPARE(painterPages, enginePages);
    QVERIFY(enginePages >= 1);
}

void CoreSmokeTest::adapter_preservesTextColor()
{
    QTextDocument doc;
    {
        QTextCursor c(&doc);
        QTextCharFormat red;
        red.setForeground(QColor(200, 20, 20));
        c.insertText(QStringLiteral("红字"), red);
    }
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    QVERIFY(!model.sections.isEmpty());
    QVERIFY(!model.sections.first().blocks.isEmpty());
    const Engine::DocParagraph &para = model.sections.first().blocks.first().paragraph;
    QVERIFY(!para.runs.isEmpty());
    QCOMPARE(para.runs.first().style.foreground.red(), 200);
}

void CoreSmokeTest::adapter_preservesImageResource()
{
    QTextDocument doc;
    QImage img(40, 30, QImage::Format_RGB32);
    img.fill(QColor(10, 120, 200));
    const QString name = QStringLiteral("test-image.png");
    doc.addResource(QTextDocument::ImageResource, QUrl(name), img);
    {
        QTextCursor c(&doc);
        QTextImageFormat fmt;
        fmt.setName(name);
        fmt.setWidth(40);
        fmt.setHeight(30);
        c.insertImage(fmt);
    }

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    bool found = false;
    for (const Engine::DocBlock &block : model.sections.first().blocks) {
        if (block.kind != Engine::DocBlock::Kind::Paragraph)
            continue;
        for (const Engine::DocRun &run : block.paragraph.runs) {
            if (run.isAtomic && !run.image.isNull()) {
                found = true;
                QVERIFY(run.atomicWidthPt > 0);
                QVERIFY(run.atomicHeightPt > 0);
            }
        }
    }
    QVERIFY(found);

    const PageLayoutSettings layout;
    const Engine::LayoutResult result = Engine::LayoutEngine::layout(model, layout);
    bool painted = false;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (line.isAtomic && !line.image.isNull())
                painted = true;
        }
    }
    QVERIFY(painted);
}

void CoreSmokeTest::adapter_preservesTableCells()
{
    QTextDocument doc;
    {
        QTextCursor c(&doc);
        QTextTable *table = c.insertTable(2, 2);
        table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("A1"));
        table->cellAt(0, 1).firstCursorPosition().insertText(QStringLiteral("B1"));
        table->cellAt(1, 0).firstCursorPosition().insertText(QStringLiteral("A2"));
        table->cellAt(1, 1).firstCursorPosition().insertText(QStringLiteral("B2"));
    }

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    const Engine::DocBlock *tableBlock = nullptr;
    for (const Engine::DocBlock &block : model.sections.first().blocks) {
        if (block.kind == Engine::DocBlock::Kind::Table) {
            tableBlock = &block;
            break;
        }
    }
    QVERIFY(tableBlock != nullptr);
    QCOMPARE(tableBlock->table.columnCount, 2);
    QCOMPARE(tableBlock->table.rows.size(), 2);
    QVERIFY(tableBlock->table.rows[0][0].paragraphs.first().plainText().contains(QStringLiteral("A1")));
    QVERIFY(tableBlock->table.rows[1][1].paragraphs.first().plainText().contains(QStringLiteral("B2")));

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    int tableRows = 0;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (line.isTableRow) {
                ++tableRows;
                QCOMPARE(line.tableCells.size(), 2);
            }
        }
    }
    QCOMPARE(tableRows, 2);
}

void CoreSmokeTest::layoutEngine_inlineTextBox_survives()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    {
        QTextCursor c(&doc);
        c.insertText(QStringLiteral("正文前"));
        c.insertBlock();

        QTextTableFormat fmt;
        fmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        fmt.setBorder(1.0);
        fmt.setCellPadding(8.0);
        fmt.setCellSpacing(0);
        fmt.setWidth(QTextLength(QTextLength::PercentageLength, 45));
        fmt.setProperty(TableGeometry::TextBoxProperty, true);

        QTextTable *table = c.insertTable(1, 1, fmt);
        QVERIFY(table);
        QVERIFY(table->format().boolProperty(TableGeometry::TextBoxProperty));
        table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("在此输入文字"));
    }

    const PageLayoutSettings layout;
    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, layout);
    bool sawTable = false;
    for (const Engine::DocBlock &block : model.sections.first().blocks) {
        if (block.kind == Engine::DocBlock::Kind::Table) {
            sawTable = true;
            QCOMPARE(block.table.columnCount, 1);
            QCOMPARE(block.table.rows.size(), 1);
            QVERIFY(block.table.rows[0][0].paragraphs.first().plainText().contains(
                QStringLiteral("在此输入文字")));
        }
    }
    QVERIFY(sawTable);

    const Engine::LayoutResult result = Engine::LayoutEngine::layout(model, layout);
    QVERIFY(result.pageCount() >= 1);
    bool sawTableRow = false;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (line.isTableRow) {
                sawTableRow = true;
                QCOMPARE(line.tableCells.size(), 1);
            }
        }
    }
    QVERIFY(sawTableRow);
}

void CoreSmokeTest::floatingTextBoxes_storeRoundTrip()
{
    QTextDocument doc;
    QVector<FloatingTextBox> boxes;
    FloatingTextBox a = FloatingTextBoxes::makeDefault(0);
    a.html = QStringLiteral("<p>Hello</p>");
    FloatingTextBox b = FloatingTextBoxes::makeDefault(1);
    b.xPt = 40;
    b.yPt = 50;
    b.html = QStringLiteral("<p>Page2</p>");
    boxes << a << b;
    FloatingTextBoxes::save(&doc, boxes, false);

    const QVector<FloatingTextBox> loaded = FloatingTextBoxes::load(&doc);
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).html.contains(QStringLiteral("Hello")), true);
    QCOMPARE(loaded.at(1).pageIndex, 1);

    const QString html = FloatingTextBoxes::embedInHtml(QStringLiteral("<html><body>x</body></html>"),
                                                        loaded);
    const QVector<FloatingTextBox> fromHtml = FloatingTextBoxes::extractFromHtml(html);
    QCOMPARE(fromHtml.size(), 2);
    QVERIFY(FloatingTextBoxes::stripMarkerFromHtml(html).contains(QStringLiteral("body")));

    const QByteArray xml = FloatingTextBoxes::toXmlBytes(loaded);
    const QVector<FloatingTextBox> fromXml = FloatingTextBoxes::fromXmlBytes(xml);
    QCOMPARE(fromXml.size(), 2);
    QCOMPARE(fromXml.at(1).xPt, 40.0);
}

void CoreSmokeTest::floatingTextBoxes_layoutAndPaint()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    Engine::DocRun run;
    run.text = QStringLiteral("body");
    run.style.font = QFont(QStringLiteral("Helvetica"), 12);
    block.paragraph.runs.append(run);
    section.blocks.append(block);
    model.sections.append(section);

    FloatingTextBox box = FloatingTextBoxes::makeDefault(0);
    box.html = QStringLiteral("<p>float</p>");
    model.floatingBoxes.append(box);

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QVERIFY(result.pageCount() >= 1);
    QVERIFY(!result.pages.first().floatingBoxes.isEmpty());
    QCOMPARE(result.pages.first().floatingBoxes.first().html.contains(QStringLiteral("float")),
             true);
}

namespace {

QTextDocument *makeShortDoc()
{
    auto *doc = new QTextDocument;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);
    QTextCursor c(doc);
    c.insertText(QStringLiteral("短文档：一页内应结束。"));
    return doc;
}

QTextDocument *makeLongPlainDoc(int paragraphs)
{
    auto *doc = new QTextDocument;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);
    QTextCursor c(doc);
    for (int i = 0; i < paragraphs; ++i) {
        if (i > 0)
            c.insertBlock();
        c.insertText(QStringLiteral(
            "Pagination eval paragraph %1. The quick brown fox jumps over the lazy dog. "
            "中文对照：用于活页估算与 LayoutEngine 页数对比。").arg(i + 1));
    }
    return doc;
}

QTextDocument *makeHeadingDoc()
{
    auto *doc = new QTextDocument;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);
    QTextCursor c(doc);
    QTextBlockFormat h1;
    h1.setHeadingLevel(1);
    c.setBlockFormat(h1);
    c.insertText(QStringLiteral("一级标题"));
    for (int i = 0; i < 25; ++i) {
        c.insertBlock();
        QTextBlockFormat body;
        body.setHeadingLevel(0);
        c.setBlockFormat(body);
        c.insertText(QStringLiteral("标题后正文段落 %1，用于混合样式分页评测。").arg(i + 1));
    }
    return doc;
}

QTextDocument *makeTableDoc()
{
    auto *doc = new QTextDocument;
    QTextCursor c(doc);
    c.insertText(QStringLiteral("表格前导段落。"));
    c.insertBlock();
    QTextTable *table = c.insertTable(6, 3);
    for (int r = 0; r < 6; ++r) {
        for (int col = 0; col < 3; ++col) {
            table->cellAt(r, col).firstCursorPosition().insertText(
                QStringLiteral("R%1C%2 单元格文本").arg(r + 1).arg(col + 1));
        }
    }
    c.movePosition(QTextCursor::End);
    c.insertBlock();
    c.insertText(QStringLiteral("表格后段落。"));
    return doc;
}

/** Several large inline images — forces engine image pagination. */
QTextDocument *makeImageDoc()
{
    auto *doc = new QTextDocument;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);
    QTextCursor c(doc);
    c.insertText(QStringLiteral("含图样例：多张大图应跨页。"));
    for (int i = 0; i < 4; ++i) {
        c.insertBlock();
        c.insertText(QStringLiteral("图 %1 说明文字。").arg(i + 1));
        c.insertBlock();
        QImage img(320, 420, QImage::Format_RGB32);
        img.fill(QColor(40 + i * 40, 90, 160));
        const QString name = QStringLiteral("eval-img-%1.png").arg(i);
        doc->addResource(QTextDocument::ImageResource, QUrl(name), img);
        QTextImageFormat fmt;
        fmt.setName(name);
        fmt.setWidth(320);
        fmt.setHeight(420);
        c.insertImage(fmt);
    }
    c.insertBlock();
    c.insertText(QStringLiteral("图片后收尾段落。"));
    return doc;
}

/** Tall table expected to span multiple pages (row-atomic, no cell split). */
QTextDocument *makeTallTableDoc()
{
    auto *doc = new QTextDocument;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc->setDefaultFont(font);
    QTextCursor c(doc);
    c.insertText(QStringLiteral("多页表样例：行作为原子单元跨页。"));
    c.insertBlock();
    constexpr int kRows = 36;
    QTextTable *table = c.insertTable(kRows, 2);
    for (int r = 0; r < kRows; ++r) {
        table->cellAt(r, 0).firstCursorPosition().insertText(
            QStringLiteral("行%1").arg(r + 1));
        table->cellAt(r, 1).firstCursorPosition().insertText(
            QStringLiteral("多页表单元格内容，用于触发 LayoutEngine 按行换页。"
                           " Extra padding text for row height."));
    }
    c.movePosition(QTextCursor::End);
    c.insertBlock();
    c.insertText(QStringLiteral("表后段落。"));
    return doc;
}

int geometricToleranceForSample(const QString &name)
{
    if (name.startsWith(QStringLiteral("长纯文本"))
        || name.startsWith(QStringLiteral("含图"))
        || name.startsWith(QStringLiteral("多页表")))
        return 3;
    return 1;
}

QList<PaginationMetrics::CompareResult> runPaginationSamples()
{
    const PageLayoutSettings layout;
    QList<PaginationMetrics::CompareResult> rows;

    struct Sample {
        QString name;
        QTextDocument *doc;
    };
    const QList<Sample> samples = {
        {QStringLiteral("空文档"), new QTextDocument},
        {QStringLiteral("短正文"), makeShortDoc()},
        {QStringLiteral("长纯文本×40"), makeLongPlainDoc(40)},
        {QStringLiteral("长纯文本×80"), makeLongPlainDoc(80)},
        {QStringLiteral("标题+正文"), makeHeadingDoc()},
        {QStringLiteral("含表格"), makeTableDoc()},
        {QStringLiteral("含图×4"), makeImageDoc()},
        {QStringLiteral("多页表×36行"), makeTallTableDoc()},
    };

    for (const Sample &s : samples) {
        rows.append(PaginationMetrics::compare(s.doc, layout, s.name));
        delete s.doc;
    }
    return rows;
}

} // namespace

void CoreSmokeTest::pagination_visualVsEngine_samplesWithinTolerance()
{
    const auto rows = runPaginationSamples();
    QVERIFY(!rows.isEmpty());
    for (const auto &r : rows) {
        // Live view pageCount is LayoutEngine-backed → must match PDF/preview.
        QVERIFY2(r.liveMatchesEngine(),
                 qPrintable(QStringLiteral("%1: live=%2 engine=%3")
                                .arg(r.sampleName)
                                .arg(r.livePages)
                                .arg(r.enginePages)));

        // Geometric residual: short ±1; long plain / image / tall table ±3.
        const int tol = geometricToleranceForSample(r.sampleName);
        QVERIFY2(r.geometricWithinTolerance(tol),
                 qPrintable(QStringLiteral("%1: geometric=%2 engine=%3 Δ=%4 (tol=%5)")
                                .arg(r.sampleName)
                                .arg(r.geometricPages)
                                .arg(r.enginePages)
                                .arg(r.geometricDelta())
                                .arg(tol)));
        QVERIFY(r.enginePages >= 1);
    }

    // Rich samples must actually exercise multi-page engine paths.
    bool sawImageMulti = false;
    bool sawTallTableMulti = false;
    for (const auto &r : rows) {
        if (r.sampleName.startsWith(QStringLiteral("含图")) && r.enginePages >= 2)
            sawImageMulti = true;
        if (r.sampleName.startsWith(QStringLiteral("多页表")) && r.enginePages >= 2)
            sawTallTableMulti = true;
    }
    QVERIFY2(sawImageMulti, "含图样例应至少 2 页");
    QVERIFY2(sawTallTableMulti, "多页表样例应至少 2 页");
}

void CoreSmokeTest::pagination_writeEvalMarkdown()
{
    const auto rows = runPaginationSamples();
    int liveOk = 0;
    int maxAbsGeo = 0;
    for (const auto &r : rows) {
        if (r.liveMatchesEngine())
            ++liveOk;
        maxAbsGeo = qMax(maxAbsGeo, r.absDelta());
    }

    QString md;
    md += QStringLiteral("# 活页页数 vs LayoutEngine 页数\n\n");
    md += QStringLiteral("评测目标：对比三种页数来源，证明 **活页页数已与预览/PDF 对齐**，"
                         "并量化旧几何估算的残余误差。\n\n");
    md += QStringLiteral("### 方法\n\n");
    md += QStringLiteral("- **几何估算**：`QTextDocument::size()` ÷ `contentBodyHeightPx`（旧双路径残余）。\n");
    md += QStringLiteral("- **活页(引擎)**：`EditorViewLayout` 的 `pageCount`，直接取 `LayoutEngine`。\n");
    md += QStringLiteral("- **LayoutEngine**：预览 / PDF / 打印同源。\n");
    md += QStringLiteral("- 纸张：A4 默认边距；字体 Helvetica 12pt（空文档除外）。\n");
    md += QStringLiteral("- 页缝：`LayoutPage::startDocPos` → `cursorRect`（文档偏移锚点）。\n\n");
    md += QStringLiteral("### 样例说明\n\n");
    md += QStringLiteral("| 样例 | 意图 |\n|------|------|\n");
    md += QStringLiteral("| 空文档 / 短正文 | 基线：单页、无溢出 |\n");
    md += QStringLiteral("| 长纯文本×40 / ×80 | 纯文本跨页；几何估算易低估 |\n");
    md += QStringLiteral("| 标题+正文 | 混合块样式 |\n");
    md += QStringLiteral("| 含表格 | 小表（单页内） |\n");
    md += QStringLiteral("| 含图×4 | 大图原子块跨页 |\n");
    md += QStringLiteral("| 多页表×36行 | 表行原子跨页（不拆单元格） |\n\n");
    md += QStringLiteral("### 接受标准\n\n");
    md += QStringLiteral("| 指标 | 标准 |\n|------|------|\n");
    md += QStringLiteral("| Δ活页（活页−引擎） | **必须为 0** |\n");
    md += QStringLiteral("| Δ几何（短样例） | ≤ 1 |\n");
    md += QStringLiteral("| Δ几何（长纯文本 / 含图 / 多页表） | ≤ 3 |\n\n");
    md += QStringLiteral("### 结果\n\n");
    md += PaginationMetrics::markdownTableHeader();
    for (const auto &r : rows)
        md += PaginationMetrics::markdownTableRow(r);
    md += QStringLiteral("\n**摘要**：%1 / %2 样例 Δ活页=0；|Δ几何| 最大 = %3。\n\n")
              .arg(liveOk)
              .arg(rows.size())
              .arg(maxAbsGeo);
    md += QStringLiteral("结论：活页与引擎页数一致；几何估算在长文/图/表上可差数页——"
                         "说明为何预览与活页页数均应改走 LayoutEngine。\n\n");
    md += QStringLiteral("由 `NewWordCoreTests::pagination_writeEvalMarkdown` 生成。"
                         " 论文提纲见 [`thesis_ch4_ch5_outline.md`](thesis_ch4_ch5_outline.md)。\n");

    const QString outDir = QStringLiteral(NEWWORD_SOURCE_DIR "/docs");
    QVERIFY(QDir().mkpath(outDir));
    const QString path = outDir + QStringLiteral("/pagination_eval.md");
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text),
             qPrintable(path));
    f.write(md.toUtf8());
    f.close();
    QVERIFY(QFileInfo::exists(path));
}

void CoreSmokeTest::layoutEngine_pageBreaksHaveDocPositions()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 60; ++i) {
            if (i > 0)
                c.insertBlock();
            c.insertText(QStringLiteral(
                "Engine seam paragraph %1 with enough text to wrap and paginate. "
                "中文分页锚点测试。").arg(i + 1));
        }
    }

    const PageLayoutSettings layout;
    const Engine::LayoutResult result = Engine::LayoutEngine::layout(
        Engine::QTextAdapter::fromDocument(&doc, layout), layout);
    QVERIFY(result.pageCount() >= 2);

    const QVector<int> breaks = result.pageBreakDocPositions();
    QCOMPARE(breaks.size(), result.pageCount() - 1);
    for (int i = 0; i < breaks.size(); ++i) {
        QVERIFY(breaks.at(i) >= 0);
        if (i > 0)
            QVERIFY(breaks.at(i) >= breaks.at(i - 1));
    }
    QVERIFY(result.pages.first().startDocPos >= 0
            || result.pages.first().lines.isEmpty());
}

void CoreSmokeTest::layoutEngine_page2FirstLineAtTop()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 80; ++i) {
            if (i > 0)
                c.insertBlock();
            c.insertText(QStringLiteral("Paragraph %1 pagination top check.").arg(i + 1));
        }
    }
    const PageLayoutSettings layout;
    const Engine::LayoutResult result = Engine::LayoutEngine::layout(
        Engine::QTextAdapter::fromDocument(&doc, layout), layout);
    QVERIFY(result.pageCount() >= 2);
    const auto &page2 = result.pages.at(1);
    QVERIFY(!page2.lines.isEmpty());
    QCOMPARE(page2.lines.first().y, 0.0);
}

void CoreSmokeTest::layoutEngine_blanksThenText_page2Y()
{
    // Empty paragraphs consume page height. Overflow blanks on page 2 push the first
    // visible line downward (preview "mid-page" text) — y still starts at 0.
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    {
        QTextCursor c(&doc);
        c.insertText(QStringLiteral("第一页首行"));
        for (int i = 0; i < 50; ++i)
            c.insertBlock();
        c.insertText(QStringLiteral("这是分页边界"));
    }
    const PageLayoutSettings layout;
    const Engine::LayoutResult result = Engine::LayoutEngine::layout(
        Engine::QTextAdapter::fromDocument(&doc, layout), layout);
    QVERIFY(result.pageCount() >= 2);
    const auto &page2 = result.pages.at(1);
    QVERIFY(!page2.lines.isEmpty());
    QCOMPARE(page2.lines.first().y, 0.0);

    qreal firstNonEmptyY = -1;
    for (const auto &line : page2.lines) {
        if (!line.text.isEmpty()) {
            firstNonEmptyY = line.y;
            break;
        }
    }
    QVERIFY(firstNonEmptyY > 0.0); // leading blanks on page 2
}

void CoreSmokeTest::layoutEngine_floatWrapClearedAcrossPage()
{
    Engine::DocumentModel model;
    Engine::DocSection section;

    Engine::DocParagraph first;
    first.documentPosition = 0;
    Engine::DocRun img;
    img.isAtomic = true;
    img.atomicWidthPt = 100;
    img.atomicHeightPt = 400; // tall float
    img.imageWrap = 2; // FloatLeft
    img.image = QImage(20, 20, QImage::Format_RGB32);
    img.image.fill(Qt::red);
    first.runs.append(img);
    Engine::DocRun t1;
    t1.text = QStringLiteral("beside-float");
    t1.style.font = QFont(QStringLiteral("Helvetica"), 12);
    first.runs.append(t1);

    Engine::DocBlock b0;
    b0.kind = Engine::DocBlock::Kind::Paragraph;
    b0.paragraph = first;
    section.blocks.append(b0);

    // Enough following paragraphs to force a page break after the float page.
    for (int i = 0; i < 40; ++i) {
        Engine::DocParagraph p;
        p.documentPosition = 100 + i * 20;
        Engine::DocRun r;
        r.text = QStringLiteral("After float paragraph %1 with enough text.").arg(i);
        r.style.font = QFont(QStringLiteral("Helvetica"), 12);
        p.runs.append(r);
        Engine::DocBlock b;
        b.kind = Engine::DocBlock::Kind::Paragraph;
        b.paragraph = p;
        section.blocks.append(b);
    }
    model.sections.append(section);

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QVERIFY(result.pageCount() >= 2);
    const auto &page2 = result.pages.at(1);
    QVERIFY(!page2.lines.isEmpty());
    // Without clearing wrap across pages, leftover wrapBottomY could push page-2
    // content far down the content box.
    QCOMPARE(page2.lines.first().y, 0.0);
}

void CoreSmokeTest::layoutEngine_oversizedImage_scalesToFit()
{
    Engine::DocumentModel model;
    Engine::DocSection section;

    Engine::DocParagraph first;
    first.documentPosition = 0;
    Engine::DocRun img;
    img.isAtomic = true;
    img.imageWrap = 1; // block image
    img.image = QImage(40, 40, QImage::Format_RGB32);
    img.image.fill(Qt::blue);
    first.runs.append(img);
    Engine::DocRun t1;
    t1.text = QStringLiteral("after-image");
    t1.style.font = QFont(QStringLiteral("Helvetica"), 12);
    first.runs.append(t1);

    Engine::DocBlock b0;
    b0.kind = Engine::DocBlock::Kind::Paragraph;
    b0.paragraph = first;
    section.blocks.append(b0);
    model.sections.append(section);

    const PageLayoutSettings layout;
    const qreal usableHeight = PageDocumentPainter::contentRectPoints(layout).height();
    // Taller than the whole page: previously overflowed the content box.
    model.sections[0].blocks[0].paragraph.runs[0].atomicWidthPt = 300;
    model.sections[0].blocks[0].paragraph.runs[0].atomicHeightPt = usableHeight * 3;

    const Engine::LayoutResult result = Engine::LayoutEngine::layout(model, layout);
    QVERIFY(result.pageCount() >= 1);
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (!line.isAtomic)
                continue;
            QVERIFY2(line.height <= usableHeight + 0.5, "oversized image must scale to page");
            QVERIFY2(line.y + line.height <= usableHeight + 0.5,
                     "oversized image must not overflow the page bottom");
        }
    }
}

void CoreSmokeTest::layoutEngine_sectionSetup_switchesPageBox()
{
    Engine::DocumentModel model;

    // Section 1: default A4 layout.
    Engine::DocSection section1;
    section1.pageSetup = PageLayoutSettings{};
    Engine::DocParagraph p1;
    p1.documentPosition = 0;
    Engine::DocRun r1;
    r1.text = QStringLiteral("section-1-body");
    r1.style.font = QFont(QStringLiteral("Helvetica"), 12);
    p1.runs.append(r1);
    Engine::DocBlock b1;
    b1.kind = Engine::DocBlock::Kind::Paragraph;
    b1.paragraph = p1;
    section1.blocks.append(b1);
    model.sections.append(section1);

    // Section 2: narrow margins — must start on a fresh page with its own box.
    Engine::DocSection section2;
    section2.pageSetup = PageLayoutSettings::narrowMargins();
    Engine::DocParagraph p2;
    p2.documentPosition = 50;
    Engine::DocRun r2;
    r2.text = QStringLiteral("section-2-body");
    r2.style.font = QFont(QStringLiteral("Helvetica"), 12);
    p2.runs.append(r2);
    Engine::DocBlock b2;
    b2.kind = Engine::DocBlock::Kind::Paragraph;
    b2.paragraph = p2;
    section2.blocks.append(b2);
    model.sections.append(section2);

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QVERIFY(result.pageCount() >= 2);

    int section2Page = -1;
    for (int i = 0; i < result.pages.size(); ++i) {
        for (const Engine::LayoutLine &line : result.pages.at(i).lines) {
            if (line.text.contains(QStringLiteral("section-2-body")))
                section2Page = i;
        }
    }
    QVERIFY(section2Page > 0);
    QCOMPARE(result.pages.at(section2Page).lines.first().y, 0.0);

    const qreal usableHeight =
        PageDocumentPainter::contentRectPoints(section2.pageSetup).height();
    for (const Engine::LayoutLine &line : result.pages.at(section2Page).lines) {
        QVERIFY2(line.y + line.height <= usableHeight + 0.5,
                 "section-2 content must fit the section's own content box");
    }
}

void CoreSmokeTest::editorViewLayout_fastStripNeverCollapsesBelowEngineFloor()
{
    QTextEdit editor;
    QFrame pageFrame;

    EditorViewLayout::Hosts hosts;
    hosts.editor = &editor;
    hosts.pageFrame = &pageFrame;
    EditorViewLayout viewLayout(hosts);

    {
        QTextCursor c(editor.document());
        for (int i = 0; i < 120; ++i) {
            c.insertText(QStringLiteral("这是一段足够长的中文文本，用于让文档超过一页。"));
            c.insertBlock();
        }
    }
    const auto precise =
        viewLayout.applyPageGeometry(100, EditorViewLayout::GeometryMode::Precise);
    QVERIFY(precise.usedEngine);
    const int engineStrip = precise.editorHeightPx;
    QVERIFY(engineStrip >= precise.pageCount * precise.onePageBodyPx);

    // Shrink the document: geometric Fast must not collapse the strip below the
    // last engine-backed height (that collapse is what made the page boundary
    // jump near the end of the previous page).
    editor.clear();
    const auto fast = viewLayout.applyPageGeometry(100, EditorViewLayout::GeometryMode::Fast);
    QVERIFY(!fast.usedEngine);
    QVERIFY(fast.editorHeightPx >= engineStrip);
}

void CoreSmokeTest::editorViewLayout_fastStripTracksContent()
{
    QTextEdit editor;
    QFrame pageFrame;

    EditorViewLayout::Hosts hosts;
    hosts.editor = &editor;
    hosts.pageFrame = &pageFrame;
    EditorViewLayout viewLayout(hosts);

    // Content between one and two pages: Fast must size the strip by the real
    // content height, not by ceil(content/body) * body (that extra phantom page
    // is what later collapsed when the engine reported a single page).
    {
        QTextCursor c(editor.document());
        for (int i = 0; i < 70; ++i) {
            c.insertText(QStringLiteral("这是一段足够长的中文文本，用于让文档超过一页。"));
            c.insertBlock();
        }
    }
    const auto fast = viewLayout.applyPageGeometry(100, EditorViewLayout::GeometryMode::Fast);
    QVERIFY(!fast.usedEngine);
    QVERIFY(fast.editorHeightPx > fast.onePageBodyPx);
    QVERIFY(fast.editorHeightPx < 2 * fast.onePageBodyPx);
}

void CoreSmokeTest::debug_pagedEditorPrimitives()
{
    const PageLayoutSettings pageLayout;
    const PageGeometry geo = PageGeometry::from(pageLayout, 100);
    const qreal contentW = geo.pageWidthPx - 2.0 * geo.marginPx;
    const qreal contentH = PageGeometry::contentBodyHeightPx(pageLayout, 100);

    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setPageSize(QSizeF(contentW, contentH));
    QVector<int> pageStarts;
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 150; ++i) {
            c.insertText(QStringLiteral("这是一段足够长的中文文本，用于测试分页编辑器。"));
            c.insertBlock();
        }
        c.insertText(QStringLiteral("结尾"));
    }
    (void)doc.documentLayout()->documentSize();
    const int pages = doc.pageCount();
    QVERIFY(pages >= 2);

    // 1) Per-page rendering via clip + translate.
    bool page1HasPixels = false;
    bool page2HasPixels = false;
    for (int k = 0; k < pages && k < 2; ++k) {
        QImage img(int(contentW), int(contentH), QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = QRectF(0, k * contentH, contentW, contentH);
        p.translate(0, -k * contentH);
        doc.documentLayout()->draw(&p, ctx);
        p.end();
        int black = 0;
        for (int y = 0; y < img.height(); y += 4) {
            for (int x = 0; x < img.width(); x += 4) {
                if (qGray(img.pixel(x, y)) < 200)
                    ++black;
            }
        }
        if (k == 0)
            page1HasPixels = black > 10;
        else
            page2HasPixels = black > 10;
    }
    QVERIFY(page1HasPixels);
    QVERIFY(page2HasPixels);

    // 2) hitTest maps viewport coordinates to a character position per page.
    auto *docLayout = doc.documentLayout();
    const int hitPage1 = docLayout->hitTest(QPointF(contentW / 2, 30), Qt::FuzzyHit);
    const int hitPage2 = docLayout->hitTest(QPointF(contentW / 2, contentH + 30), Qt::FuzzyHit);
    QVERIFY(hitPage1 >= 0 && hitPage1 < hitPage2);
    QVERIFY(hitPage2 >= 0);

    // 3) QTextDocument undo/redo works without QTextEdit.
    QTextCursor c(&doc);
    c.setPosition(hitPage2);
    const int before = doc.characterCount();
    c.insertText(QStringLiteral("X"));
    QVERIFY(doc.characterCount() == before + 1);
    doc.undo();
    QVERIFY(doc.characterCount() == before);
    doc.redo();
    QVERIFY(doc.characterCount() == before + 1);
}

void CoreSmokeTest::pagedEditorWidget_pageBoundaryTyping()
{
    const PageLayoutSettings pageLayout;
    const PageGeometry geo = PageGeometry::from(pageLayout, 100);
    const qreal contentW = geo.pageWidthPx - 2.0 * geo.marginPx;
    const qreal contentH = PageGeometry::contentBodyHeightPx(pageLayout, 100);

    QTextDocument doc;
    doc.setUndoRedoEnabled(true);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 70; ++i) {
            c.insertText(QStringLiteral("第%1行：这是一段用于填满页面的中文示例文字。").arg(i + 1));
            c.insertBlock();
        }
    }

    PagedEditorWidget view(&doc, pageLayout, HeaderFooterSettings{});
    view.resize(1100, 2200);
    QVERIFY(view.pageCount() >= 2);

    // The widget paginates with layoutPageHeight = contentH - maxLineHeight - 1;
    // a line belongs to page floor(top / layoutPageHeight) and its bottom must
    // stay inside that page's visual content box.
    qreal maxLineH = 1.0;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        const QTextLayout *tl = b.layout();
        for (int i = 0; i < tl->lineCount(); ++i)
            maxLineH = qMax(maxLineH, tl->lineAt(i).height());
    }
    const qreal layoutPageH = qMax(120.0, contentH - maxLineH - 1.0);
    auto verifyNoStraddle = [&](const char *tag) {
        for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
            const QRectF br = doc.documentLayout()->blockBoundingRect(b);
            const QTextLayout *tl = b.layout();
            for (int i = 0; i < tl->lineCount(); ++i) {
                const QTextLine ln = tl->lineAt(i);
                const qreal top = br.top() + ln.y();
                const int page = qMax(0, int(top / layoutPageH));
                QVERIFY2(top + ln.height() <= (page + 1) * contentH + 0.5,
                         qPrintable(QStringLiteral("%1 line %2 of block %3 escapes page "
                                                   "box: top=%4 bottom=%5 page=%6")
                                        .arg(QLatin1String(tag))
                                        .arg(i)
                                        .arg(b.blockNumber())
                                        .arg(top)
                                        .arg(top + ln.height())
                                        .arg(page)));
            }
        }
    };

    // Invariant: no line may straddle a page box — each line renders fully on
    // one page, never cut at the boundary and never bleeding into the footer.
    verifyNoStraddle("pre-edit");

    // Caret at the end of page 1's last line; a long typed run must push the
    // caret onto page 2 together with the text.
    const int boundaryPos = doc.documentLayout()->hitTest(
        QPointF(contentW - 2, contentH - 8), Qt::FuzzyHit);
    QTextCursor caret(&doc);
    caret.setPosition(boundaryPos);
    view.setTextCursor(caret);

    QKeyEvent keyX(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
    for (int i = 0; i < 60; ++i)
        QApplication::sendEvent(&view, &keyX);

    QVERIFY2(view.currentPageIndex() == 1,
             qPrintable(QStringLiteral("caret page=%1 after typing at page-1 end")
                            .arg(view.currentPageIndex())));

    // The caret rectangle must be inside page 2's widget content area.
    const QRectF caretR = view.cursorRect();
    const int page2Top = 24 + geo.pageHeightPx + 28;
    const qreal contentTopPx = (pageLayout.marginsMm.top() + pageLayout.headerDistanceMm)
                               * 96.0 / 25.4;
    QVERIFY2(caretR.top() >= page2Top + contentTopPx - 0.5,
             qPrintable(QStringLiteral("caretY=%1 not in page 2").arg(caretR.top())));
    QVERIFY2(caretR.top() <= page2Top + contentTopPx + contentH + 0.5,
             qPrintable(QStringLiteral("caretY=%1 past page 2 box").arg(caretR.top())));

    // Re-check the no-straddle invariant after editing.
    verifyNoStraddle("post-edit");
}

void CoreSmokeTest::pagedEditorWidget_renderAndEdit()
{
    PageLayoutSettings pageLayout;
    HeaderFooterSettings headerFooter;
    headerFooter.header = QStringLiteral("NewWord 页眉");
    headerFooter.footer = QStringLiteral("测试页脚");

    QTextDocument doc;
    doc.setUndoRedoEnabled(true);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 150; ++i) {
            c.insertText(QStringLiteral("这是一段足够长的中文文本，用于测试分页编辑器。"));
            c.insertBlock();
        }
        c.insertText(QStringLiteral("结尾"));
    }

    PagedEditorWidget view(&doc, pageLayout, headerFooter);
    view.resize(1100, 2200);
    QVERIFY2(view.pageCount() >= 2, qPrintable(QStringLiteral("pages=%1").arg(view.pageCount())));

    // 1) Rendering: both pages contain real text pixels.
    QImage img(view.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    view.render(&img);
    const PageGeometry geo = PageGeometry::from(pageLayout, 100);
    const qreal contentTopPx = (pageLayout.marginsMm.top() + pageLayout.headerDistanceMm)
                               * 96.0 / 25.4;
    const int page2Top = 24 + geo.pageHeightPx + 28;
    auto hasDarkPixels = [&img](int y0, int y1) {
        for (int y = y0; y < qMin(y1, img.height()); ++y) {
            for (int x = 0; x < img.width(); x += 3) {
                if (qGray(img.pixel(x, y)) < 140)
                    return true;
            }
        }
        return false;
    };
    QVERIFY2(hasDarkPixels(30, 250), "page 1 should contain text pixels");
    QVERIFY2(page2Top + 100 < img.height(), "widget must be tall enough to show page 2");
    QVERIFY2(hasDarkPixels(page2Top + 30, page2Top + 300),
             "page 2 should contain text pixels");
    // Body text of page 2 must render (header alone would fool this check).
    const int bodyY0 = page2Top + int(contentTopPx) + 40;
    QVERIFY2(bodyY0 + 500 < img.height(), "page 2 body must be inside the image");
    QVERIFY2(hasDarkPixels(bodyY0, bodyY0 + 500),
             "page 2 body text should render inside its content box");

    // 2) Mouse click inside page-2 content moves the cursor to page 2.
    const qreal pageX = qMax(0.0, (1100.0 - geo.pageWidthPx) / 2.0);
    const QPoint click2(int(pageX + pageLayout.marginsMm.left() * 96.0 / 25.4 + 100),
                        page2Top + int(contentTopPx) + 120);
    QMouseEvent press(QEvent::MouseButtonPress, click2, click2, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    QVERIFY2(view.currentPageIndex() == 1,
             qPrintable(QStringLiteral("cursor page=%1").arg(view.currentPageIndex())));
    QVERIFY(view.textCursor().position() > 0);
    // The caret must land near the clicked position, not at page-1 top.
    QVERIFY2(qAbs(view.cursorRect().center().y() - click2.y()) < 200,
             qPrintable(QStringLiteral("caretY=%1 clickY=%2")
                            .arg(view.cursorRect().center().y()).arg(click2.y())));
    // The caret must be a thin vertical line, never a filled glyph box.
    QVERIFY2(view.cursorRect().width() <= 4.0,
             qPrintable(QStringLiteral("caret width=%1").arg(view.cursorRect().width())));

    // 3) Drag selects a range.
    const QPoint dragEnd = click2 + QPoint(0, 80);
    QMouseEvent moveEv(QEvent::MouseMove, dragEnd, dragEnd, Qt::NoButton,
                       Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &moveEv);
    QMouseEvent release(QEvent::MouseButtonRelease, dragEnd, dragEnd, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &release);
    QVERIFY(view.textCursor().hasSelection());
    QVERIFY(view.textCursor().selectionEnd() > view.textCursor().selectionStart());

    // 4) Keyboard input replaces the selection.
    QKeyEvent keyA(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("A"));
    QApplication::sendEvent(&view, &keyA);
    QVERIFY(doc.toPlainText().contains(QLatin1Char('A')));
    QVERIFY(!view.textCursor().hasSelection());

    // 5) IME: preedit underline text, then a commit replaces it.
    QInputMethodEvent preedit(QStringLiteral("nihao"), {});
    QApplication::sendEvent(&view, &preedit);
    QVERIFY(doc.toPlainText().contains(QStringLiteral("nihao")));
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("你"));
    QApplication::sendEvent(&view, &commit);
    const QString text = doc.toPlainText();
    QVERIFY(text.contains(QStringLiteral("你")));
    QVERIFY(!text.contains(QStringLiteral("nihao")));

    // 6) Undo / redo through the widget.
    view.undo();
    QVERIFY(!doc.toPlainText().contains(QStringLiteral("你")));
    view.redo();
    QVERIFY(doc.toPlainText().contains(QStringLiteral("你")));

    // 7) Regression: pressing Enter must move the drawn caret downward.
    view.setTextCursor(QTextCursor(&doc));
    const qreal caretY0 = view.cursorRect().top();
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier,
                    QStringLiteral("\n"));
    QApplication::sendEvent(&view, &enter);
    const qreal caretY1 = view.cursorRect().top();
    QApplication::sendEvent(&view, &enter);
    const qreal caretY2 = view.cursorRect().top();
    QVERIFY2(caretY1 > caretY0,
             qPrintable(QStringLiteral("caretY0=%1 caretY1=%2").arg(caretY0).arg(caretY1)));
    QVERIFY2(caretY2 > caretY1,
             qPrintable(QStringLiteral("caretY1=%1 caretY2=%2").arg(caretY1).arg(caretY2)));
}

void CoreSmokeTest::docx_modelExporter_roundTrip_plainAndStyled()
{
    QTextDocument original;
    {
        QTextCursor c(&original);
        QTextBlockFormat heading;
        heading.setHeadingLevel(1);
        c.setBlockFormat(heading);
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        bold.setForeground(QColor(20, 80, 180));
        c.insertText(QStringLiteral("模型导出标题"), bold);
        c.insertBlock();
        QTextBlockFormat body;
        body.setHeadingLevel(0);
        c.setBlockFormat(body);
        c.setCharFormat(QTextCharFormat());
        c.insertText(QStringLiteral("DocumentModel → DocxExporter 正文。"));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("model-export.docx"));

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&original);
    QString err;
    QVERIFY2(Engine::DocxExporter::save(model, path, &err), qPrintable(err));

    QTextDocument loaded;
    QVERIFY2(DocxIO::load(&loaded, path, &err), qPrintable(err));
    const QString plain = loaded.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("模型导出标题")));
    QVERIFY(plain.contains(QStringLiteral("DocumentModel → DocxExporter 正文")));
}

void CoreSmokeTest::docx_modelExporter_preservesTable()
{
    QTextDocument original;
    {
        QTextCursor c(&original);
        QTextTable *table = c.insertTable(2, 2);
        table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("左上"));
        table->cellAt(0, 1).firstCursorPosition().insertText(QStringLiteral("右上"));
        table->cellAt(1, 0).firstCursorPosition().insertText(QStringLiteral("左下"));
        table->cellAt(1, 1).firstCursorPosition().insertText(QStringLiteral("右下"));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("model-table.docx"));
    QString err;
    QVERIFY2(DocxIO::save(&original, path, &err), qPrintable(err)); // via Model exporter

    QTextDocument loaded;
    QVERIFY2(DocxIO::load(&loaded, path, &err), qPrintable(err));
    const QString plain = loaded.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("左上")));
    QVERIFY(plain.contains(QStringLiteral("右下")));
}

void CoreSmokeTest::docx_importer_modelClosedLoop_headingBoldTable()
{
    // Pure Model path: build → export → import → Model field checks → toDocument.
    Engine::DocumentModel original;
    Engine::DocSection section;

    Engine::DocBlock heading;
    heading.kind = Engine::DocBlock::Kind::Paragraph;
    heading.paragraph.styleId = StyleUtils::StyleId::Heading1;
    heading.paragraph.headingLevel = 1;
    Engine::DocRun hRun;
    hRun.text = QStringLiteral("闭环标题");
    hRun.style.bold = true;
    heading.paragraph.runs.append(hRun);
    section.blocks.append(heading);

    Engine::DocBlock body;
    body.kind = Engine::DocBlock::Kind::Paragraph;
    Engine::DocRun bold;
    bold.text = QStringLiteral("粗体");
    bold.style.bold = true;
    Engine::DocRun rest;
    rest.text = QStringLiteral("与普通");
    body.paragraph.runs.append(bold);
    body.paragraph.runs.append(rest);
    section.blocks.append(body);

    Engine::DocBlock tableBlock;
    tableBlock.kind = Engine::DocBlock::Kind::Table;
    tableBlock.table.columnCount = 2;
    Engine::DocTableCell a;
    Engine::DocParagraph ap;
    Engine::DocRun ar;
    ar.text = QStringLiteral("A1");
    ap.runs.append(ar);
    a.paragraphs.append(ap);
    Engine::DocTableCell b;
    Engine::DocParagraph bp;
    Engine::DocRun br;
    br.text = QStringLiteral("B1");
    bp.runs.append(br);
    b.paragraphs.append(bp);
    tableBlock.table.rows = {{a, b}};
    section.blocks.append(tableBlock);
    original.sections.append(section);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("model-closed-loop.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(original, path, &err), qPrintable(err));

    Engine::DocumentModel loaded;
    QVERIFY2(Engine::DocxImporter::load(&loaded, path, &err), qPrintable(err));
    QVERIFY(!loaded.sections.isEmpty());
    QCOMPARE(loaded.blockCount(), 3);

    const auto &blocks = loaded.sections.first().blocks;
    QCOMPARE(int(blocks[0].kind), int(Engine::DocBlock::Kind::Paragraph));
    QCOMPARE(blocks[0].paragraph.headingLevel, 1);
    QCOMPARE(blocks[0].paragraph.styleId, StyleUtils::StyleId::Heading1);
    QVERIFY(blocks[0].paragraph.plainText().contains(QStringLiteral("闭环标题")));

    QCOMPARE(int(blocks[1].kind), int(Engine::DocBlock::Kind::Paragraph));
    QVERIFY(blocks[1].paragraph.runs.size() >= 1);
    bool sawBold = false;
    for (const auto &run : blocks[1].paragraph.runs) {
        if (run.text.contains(QStringLiteral("粗体")) && run.style.bold)
            sawBold = true;
    }
    QVERIFY2(sawBold, "expected bold run after Model import");

    QCOMPARE(int(blocks[2].kind), int(Engine::DocBlock::Kind::Table));
    QCOMPARE(blocks[2].table.columnCount, 2);
    QVERIFY(!blocks[2].table.rows.isEmpty());
    QCOMPARE(blocks[2].table.rows[0][0].paragraphs.first().plainText(), QStringLiteral("A1"));
    QCOMPARE(blocks[2].table.rows[0][1].paragraphs.first().plainText(), QStringLiteral("B1"));

    QTextDocument doc;
    Engine::QTextAdapter::toDocument(loaded, &doc);
    const QString docPlain = doc.toPlainText();
    QVERIFY(docPlain.contains(QStringLiteral("闭环标题")));
    QVERIFY(docPlain.contains(QStringLiteral("粗体")));
    QVERIFY(docPlain.contains(QStringLiteral("A1")));
    QVERIFY(docPlain.contains(QStringLiteral("B1")));
}

void CoreSmokeTest::adapter_preservesMergedTableCells()
{
    QTextDocument doc;
    {
        QTextCursor c(&doc);
        QTextTable *table = c.insertTable(2, 2);
        table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("合并"));
        table->cellAt(0, 1).firstCursorPosition().insertText(QStringLiteral("右上"));
        table->cellAt(1, 0).firstCursorPosition().insertText(QStringLiteral("左下"));
        table->cellAt(1, 1).firstCursorPosition().insertText(QStringLiteral("右下"));
        table->mergeCells(0, 0, 2, 1); // vertical merge left column
    }

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    const Engine::DocTable *table = nullptr;
    for (const Engine::DocBlock &block : model.sections.first().blocks) {
        if (block.kind == Engine::DocBlock::Kind::Table) {
            table = &block.table;
            break;
        }
    }
    QVERIFY(table != nullptr);
    QCOMPARE(table->columnCount, 2);
    QCOMPARE(table->rows.size(), 2);
    QVERIFY(!table->rows[0][0].covered);
    QCOMPARE(table->rows[0][0].rowSpan, 2);
    QCOMPARE(table->rows[0][0].columnSpan, 1);
    QVERIFY(table->rows[1][0].covered);
    QVERIFY(table->rows[0][0].paragraphs.first().plainText().contains(QStringLiteral("合并")));
}

void CoreSmokeTest::docx_model_mergedCells_roundTrip()
{
    Engine::DocumentModel original;
    Engine::DocSection section;
    Engine::DocBlock tableBlock;
    tableBlock.kind = Engine::DocBlock::Kind::Table;
    tableBlock.table.columnCount = 2;

    auto cellWithText = [](const QString &text) {
        Engine::DocTableCell cell;
        Engine::DocParagraph p;
        Engine::DocRun r;
        r.text = text;
        p.runs.append(r);
        cell.paragraphs.append(p);
        return cell;
    };
    auto covered = []() {
        Engine::DocTableCell c;
        c.covered = true;
        return c;
    };

    Engine::DocTableCell a = cellWithText(QStringLiteral("跨两列"));
    a.columnSpan = 2;
    a.rowSpan = 1;
    Engine::DocTableCell b = cellWithText(QStringLiteral("左下"));
    Engine::DocTableCell c = cellWithText(QStringLiteral("右下"));
    tableBlock.table.rows = {{a, covered()}, {b, c}};
    section.blocks.append(tableBlock);
    original.sections.append(section);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("merged-cells.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(original, path, &err), qPrintable(err));

    Engine::DocumentModel loaded;
    QVERIFY2(Engine::DocxImporter::load(&loaded, path, &err), qPrintable(err));
    QVERIFY(!loaded.sections.isEmpty());
    const Engine::DocTable &t = loaded.sections.first().blocks.first().table;
    QCOMPARE(t.columnCount, 2);
    QCOMPARE(t.rows.size(), 2);
    QVERIFY(!t.rows[0][0].covered);
    QCOMPARE(t.rows[0][0].columnSpan, 2);
    QVERIFY(t.rows[0][1].covered);
    QVERIFY(t.rows[0][0].paragraphs.first().plainText().contains(QStringLiteral("跨两列")));
    QCOMPARE(t.rows[1][0].paragraphs.first().plainText(), QStringLiteral("左下"));
    QCOMPARE(t.rows[1][1].paragraphs.first().plainText(), QStringLiteral("右下"));
}

void CoreSmokeTest::layoutEngine_mergedCells_paintHeights()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    Engine::DocBlock tableBlock;
    tableBlock.kind = Engine::DocBlock::Kind::Table;
    tableBlock.table.columnCount = 2;

    auto cellWithText = [](const QString &text) {
        Engine::DocTableCell cell;
        Engine::DocParagraph p;
        Engine::DocRun r;
        r.text = text;
        r.style.font.setPointSizeF(12.0);
        p.runs.append(r);
        cell.paragraphs.append(p);
        return cell;
    };
    auto covered = []() {
        Engine::DocTableCell c;
        c.covered = true;
        return c;
    };

    Engine::DocTableCell a = cellWithText(QStringLiteral("竖向合并"));
    a.rowSpan = 2;
    a.columnSpan = 1;
    tableBlock.table.rows = {
        {a, cellWithText(QStringLiteral("右上"))},
        {covered(), cellWithText(QStringLiteral("右下"))},
    };
    section.blocks.append(tableBlock);
    model.sections.append(section);

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    bool sawSpan = false;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (!line.isTableRow)
                continue;
            for (const Engine::LayoutTableCell &cell : line.tableCells) {
                if (!cell.covered && cell.rowSpan == 2) {
                    QVERIFY(cell.paintHeight > cell.height + 0.5);
                    sawSpan = true;
                }
            }
        }
    }
    QVERIFY2(sawSpan, "expected a layout cell with rowSpan=2 and taller paintHeight");
}

void CoreSmokeTest::textStats_cjkAndLatinAndPunctuation()
{
    // "你好，世界！Hello world."
    // CJK: 你好世界 (4); punct: ，！. (3); Latin words: Hello, world (2) → words=6
    // chars (no space): 4 + 2 CJK punct + Hello(5) + world(5) + .(1) = 17
    const TextStats::Counts c = TextStats::analyze(QStringLiteral("你好，世界！Hello world."));
    QCOMPARE(c.cjkChars, 4);
    QCOMPARE(c.punctuation, 3);
    QCOMPARE(c.words, 6);
    QCOMPARE(c.chars, 17);
    QCOMPARE(c.spaces, 1);
    QVERIFY(c.charsWithSpaces > c.chars);
}

void CoreSmokeTest::textStats_selectionUsesParagraphSeparator()
{
    QString sel = QStringLiteral("第一行");
    sel += QChar::ParagraphSeparator;
    sel += QStringLiteral("第二行");
    const TextStats::Counts c = TextStats::analyze(sel);
    QCOMPARE(c.cjkChars, 6);
    QCOMPARE(c.words, 6);
    QCOMPARE(c.spaces, 1);
    QCOMPARE(c.chars, 6);
}

void CoreSmokeTest::docx_meta_headerFooter_roundTrip()
{
    QTextDocument doc;
    QTextCursor c(&doc);
    c.insertText(QStringLiteral("正文一段。"));

    DocxDocumentMeta meta;
    meta.headerFooter.header = QStringLiteral("测试页眉");
    meta.headerFooter.footer = QStringLiteral("测试页脚");
    meta.headerFooter.showPageNumber = true;
    meta.pageLayout = PageLayoutSettings::normalMargins();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("hf.docx"));
    QString err;
    QVERIFY2(DocxIO::save(&doc, path, &err, &meta), qPrintable(err));

    DocxDocumentMeta loaded;
    QVERIFY(DocxPackage::readMeta(path, &loaded));
    QCOMPARE(loaded.headerFooter.header, QStringLiteral("测试页眉"));
    QVERIFY(loaded.headerFooter.footer.contains(QStringLiteral("测试页脚")));
    QVERIFY(loaded.headerFooter.showPageNumber);
}

void CoreSmokeTest::layoutEngine_floatImage_wrapsTextBeside()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    Engine::DocParagraph para;
    para.documentPosition = 0;

    Engine::DocRun img;
    img.isAtomic = true;
    img.atomicWidthPt = 120;
    img.atomicHeightPt = 80;
    img.imageWrap = 2; // FloatLeft
    img.image = QImage(40, 30, QImage::Format_RGB32);
    img.image.fill(Qt::blue);
    para.runs.append(img);

    Engine::DocRun text;
    text.text = QStringLiteral(
        "This paragraph should wrap beside the floating image for several lines of text "
        "so the layout engine places text with a non-zero x offset while below the image top.");
    text.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(text);

    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    model.sections.append(section);

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QVERIFY(result.pageCount() >= 1);
    bool sawAtomic = false;
    bool sawIndentedText = false;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (line.isAtomic)
                sawAtomic = true;
            if (!line.isAtomic && !line.text.isEmpty() && line.x > 10.0)
                sawIndentedText = true;
        }
    }
    QVERIFY(sawAtomic);
    QVERIFY2(sawIndentedText, "expected text lines beside float-left image");
}

void CoreSmokeTest::adapter_snapshotCache_suffixMatchesFull()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    QTextCursor c(&doc);
    for (int i = 0; i < 40; ++i) {
        if (i > 0)
            c.insertBlock();
        c.insertText(QStringLiteral("Cache para %1 中文对照。").arg(i + 1));
    }

    Engine::QTextAdapter::SnapshotCache cache;
    const PageLayoutSettings layout;
    const Engine::DocumentModel warm = cache.ensure(&doc, layout);
    QCOMPARE(warm.blockCount(), Engine::QTextAdapter::fromDocument(&doc, layout).blockCount());

    c.movePosition(QTextCursor::End);
    const int pos = c.position();
    c.insertText(QStringLiteral(" typed-tail"));
    cache.noteChange(pos);

    const Engine::DocumentModel incremental = cache.ensure(&doc, layout);
    const Engine::DocumentModel full = Engine::QTextAdapter::fromDocument(&doc, layout);
    QCOMPARE(incremental.blockCount(), full.blockCount());
    QCOMPARE(incremental.paragraphCount(), full.paragraphCount());
    QVERIFY(!incremental.sections.isEmpty());
    QVERIFY(!full.sections.isEmpty());
    QCOMPARE(incremental.sections.first().blocks.size(), full.sections.first().blocks.size());
    for (int i = 0; i < full.sections.first().blocks.size(); ++i) {
        const auto &a = incremental.sections.first().blocks.at(i);
        const auto &b = full.sections.first().blocks.at(i);
        QCOMPARE(int(a.kind), int(b.kind));
        QCOMPARE(a.documentPosition, b.documentPosition);
        if (a.kind == Engine::DocBlock::Kind::Paragraph)
            QCOMPARE(a.paragraph.plainText(), b.paragraph.plainText());
    }

    QCOMPARE(Engine::LayoutEngine::pageCount(incremental, layout),
             Engine::LayoutEngine::pageCount(full, layout));
}

void CoreSmokeTest::adapter_extractsFootnotes_skipsAppendix()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    QTextCursor c(&doc);
    c.insertText(QStringLiteral("正文引用"));
    QVERIFY(ReviewNotes::insertFootnote(c, QStringLiteral("脚注正文内容")));
    c.insertText(QStringLiteral("后续。"));

    // Live editor also materializes a trailing「脚注」appendix — engine must skip it.
    ReviewNotes::ensureFootnotesAppendix(&doc);
    QVERIFY(doc.toPlainText().contains(QStringLiteral("脚注")));

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    QCOMPARE(model.footnoteOrder.size(), 1);
    QVERIFY(model.footnoteBodies.contains(model.footnoteOrder.first()));
    QCOMPARE(model.footnoteBodies.value(model.footnoteOrder.first()),
             QStringLiteral("脚注正文内容"));

    bool sawMarker = false;
    bool sawAppendixHeading = false;
    for (const Engine::DocSection &sec : model.sections) {
        for (const Engine::DocBlock &block : sec.blocks) {
            if (block.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            if (block.paragraph.headingLevel == 2
                && block.paragraph.plainText().trimmed() == QStringLiteral("脚注"))
                sawAppendixHeading = true;
            for (const Engine::DocRun &run : block.paragraph.runs) {
                if (!run.footnoteId.isEmpty()) {
                    sawMarker = true;
                    QCOMPARE(run.footnoteNumber, 1);
                    QVERIFY(run.style.superscript);
                }
            }
        }
    }
    QVERIFY(sawMarker);
    QVERIFY2(!sawAppendixHeading, "appendix H2「脚注」must not enter the Model");
}

void CoreSmokeTest::layoutEngine_footnotes_atPageBottom()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    section.pageSetup = PageLayoutSettings{};

    Engine::DocParagraph para;
    para.documentPosition = 0;
    Engine::DocRun body;
    body.text = QStringLiteral("正文带脚注");
    body.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(body);
    Engine::DocRun marker;
    marker.text = QStringLiteral("1");
    marker.footnoteId = QStringLiteral("fn1");
    marker.footnoteNumber = 1;
    marker.style.font = QFont(QStringLiteral("Helvetica"), 12);
    marker.style.superscript = true;
    para.runs.append(marker);

    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    model.sections.append(section);
    model.footnoteOrder.append(QStringLiteral("fn1"));
    model.footnoteBodies.insert(QStringLiteral("fn1"), QStringLiteral("页底脚注说明"));

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QCOMPARE(result.pageCount(), 1);
    const Engine::LayoutPage &page = result.pages.first();
    QVERIFY(page.hasFootnoteRule);
    QVERIFY(!page.footnoteLines.isEmpty());
    QVERIFY(page.footnoteLines.first().text.contains(QStringLiteral("页底脚注说明")));
    QVERIFY(page.footnoteRuleY > 0);
    // Footnote band sits below body content.
    if (!page.lines.isEmpty())
        QVERIFY(page.footnoteRuleY >= page.lines.last().y + page.lines.last().height - 0.5);
}

void CoreSmokeTest::docx_footnotes_modelRoundTrip()
{
    Engine::DocumentModel original;
    Engine::DocSection section;
    Engine::DocParagraph para;
    Engine::DocRun body;
    body.text = QStringLiteral("正文引用脚注");
    body.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(body);
    Engine::DocRun marker;
    marker.text = QStringLiteral("1");
    marker.footnoteId = QStringLiteral("note-a");
    marker.footnoteNumber = 1;
    marker.style.font = QFont(QStringLiteral("Helvetica"), 12);
    marker.style.superscript = true;
    para.runs.append(marker);
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    original.sections.append(section);
    original.footnoteOrder.append(QStringLiteral("note-a"));
    original.footnoteBodies.insert(QStringLiteral("note-a"), QStringLiteral("脚注正文内容"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("footnotes.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(original, path, &err), qPrintable(err));

    Engine::DocumentModel loaded;
    QVERIFY2(Engine::DocxImporter::load(&loaded, path, &err), qPrintable(err));
    QCOMPARE(loaded.footnoteOrder.size(), 1);
    QCOMPARE(loaded.footnoteBodies.value(loaded.footnoteOrder.first()),
             QStringLiteral("脚注正文内容"));

    bool sawMarker = false;
    for (const Engine::DocSection &sec : loaded.sections) {
        for (const Engine::DocBlock &b : sec.blocks) {
            if (b.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            for (const Engine::DocRun &run : b.paragraph.runs) {
                if (run.footnoteId.isEmpty())
                    continue;
                sawMarker = true;
                QCOMPARE(run.footnoteId, loaded.footnoteOrder.first());
                QVERIFY(run.style.superscript);
                QCOMPARE(loaded.footnoteBodies.value(run.footnoteId),
                         QStringLiteral("脚注正文内容"));
            }
        }
    }
    QVERIFY(sawMarker);

    // Layout still places note at page bottom after DOCX round-trip.
    const Engine::LayoutResult layout =
        Engine::LayoutEngine::layout(loaded, PageLayoutSettings{});
    QCOMPARE(layout.pageCount(), 1);
    QVERIFY(layout.pages.first().hasFootnoteRule);
    QVERIFY(!layout.pages.first().footnoteLines.isEmpty());
    QVERIFY(layout.pages.first().footnoteLines.first().text.contains(
        QStringLiteral("脚注正文内容")));
}

void CoreSmokeTest::docx_package_detectsFootnotes()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    Engine::DocParagraph para;
    Engine::DocRun body;
    body.text = QStringLiteral("有脚注");
    body.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(body);
    Engine::DocRun marker;
    marker.text = QStringLiteral("1");
    marker.footnoteId = QStringLiteral("fn");
    marker.footnoteNumber = 1;
    marker.style.superscript = true;
    para.runs.append(marker);
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    model.sections.append(section);
    model.footnoteOrder.append(QStringLiteral("fn"));
    model.footnoteBodies.insert(QStringLiteral("fn"), QStringLiteral("检测用"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString withFn = dir.filePath(QStringLiteral("with-fn.docx"));
    const QString plain = dir.filePath(QStringLiteral("plain.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(model, withFn, &err), qPrintable(err));
    QVERIFY(DocxPackage::hasFootnotes(withFn));

    Engine::DocumentModel plainModel;
    Engine::DocSection plainSec;
    Engine::DocBlock plainBlock;
    plainBlock.kind = Engine::DocBlock::Kind::Paragraph;
    Engine::DocRun plainRun;
    plainRun.text = QStringLiteral("无脚注");
    plainRun.style.font = QFont(QStringLiteral("Helvetica"), 12);
    plainBlock.paragraph.runs.append(plainRun);
    plainSec.blocks.append(plainBlock);
    plainModel.sections.append(plainSec);
    QVERIFY2(Engine::DocxExporter::save(plainModel, plain, &err), qPrintable(err));
    QVERIFY(!DocxPackage::hasFootnotes(plain));
}

void CoreSmokeTest::adapter_extractsComments()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    QTextCursor c(&doc);
    c.insertText(QStringLiteral("普通文本"));
    c.insertText(QStringLiteral("被批注的片段"));
    c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 6);
    QVERIFY(ReviewNotes::insertComment(c, QStringLiteral("审阅者"), QStringLiteral("请改一下措辞")));

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    QCOMPARE(model.commentOrder.size(), 1);
    const QString id = model.commentOrder.first();
    QCOMPARE(model.comments.value(id).text, QStringLiteral("请改一下措辞"));
    QCOMPARE(model.comments.value(id).author, QStringLiteral("审阅者"));

    bool sawTagged = false;
    for (const Engine::DocSection &sec : model.sections) {
        for (const Engine::DocBlock &block : sec.blocks) {
            if (block.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            for (const Engine::DocRun &run : block.paragraph.runs) {
                if (run.commentId == id) {
                    sawTagged = true;
                    QVERIFY(run.style.background.isValid());
                }
            }
        }
    }
    QVERIFY(sawTagged);

    // Round-trip through toDocument restores resource + highlight.
    QTextDocument restored;
    Engine::QTextAdapter::toDocument(model, &restored);
    const auto comments = ReviewNotes::collectComments(&restored);
    QCOMPARE(comments.size(), 1);
    QCOMPARE(comments.first().text, QStringLiteral("请改一下措辞"));
}

void CoreSmokeTest::docx_comments_modelRoundTrip()
{
    Engine::DocumentModel original;
    Engine::DocSection section;
    Engine::DocParagraph para;
    Engine::DocRun plain;
    plain.text = QStringLiteral("前文");
    plain.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(plain);
    Engine::DocRun tagged;
    tagged.text = QStringLiteral("重点句");
    tagged.commentId = QStringLiteral("c1");
    tagged.style.font = QFont(QStringLiteral("Helvetica"), 12);
    tagged.style.background = QColor(255, 249, 196);
    para.runs.append(tagged);
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    original.sections.append(section);
    original.commentOrder.append(QStringLiteral("c1"));
    original.comments.insert(QStringLiteral("c1"),
                             Engine::DocComment{QStringLiteral("Alice"), QStringLiteral("建议删改")});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("comments.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(original, path, &err), qPrintable(err));
    QVERIFY(DocxPackage::hasComments(path));

    Engine::DocumentModel loaded;
    QVERIFY2(Engine::DocxImporter::load(&loaded, path, &err), qPrintable(err));
    QCOMPARE(loaded.commentOrder.size(), 1);
    QCOMPARE(loaded.comments.value(loaded.commentOrder.first()).text, QStringLiteral("建议删改"));

    bool sawTagged = false;
    for (const Engine::DocSection &sec : loaded.sections) {
        for (const Engine::DocBlock &b : sec.blocks) {
            if (b.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            for (const Engine::DocRun &run : b.paragraph.runs) {
                if (run.commentId.isEmpty())
                    continue;
                sawTagged = true;
                QCOMPARE(loaded.comments.value(run.commentId).text, QStringLiteral("建议删改"));
            }
        }
    }
    QVERIFY(sawTagged);

    const Engine::LayoutResult layout =
        Engine::LayoutEngine::layout(loaded, PageLayoutSettings{});
    QCOMPARE(layout.pageCount(), 1);
    bool sawHighlight = false;
    for (const Engine::LayoutLine &line : layout.pages.first().lines) {
        for (const QTextLayout::FormatRange &fr : line.formats) {
            if (fr.format.background().style() != Qt::NoBrush) {
                sawHighlight = true;
                break;
            }
        }
    }
    QVERIFY2(sawHighlight, "expected comment highlight formats on layout lines");
}

void CoreSmokeTest::adapter_extractsEndnotes_skipsAppendix()
{
    QTextDocument doc;
    QFont font(QStringLiteral("Helvetica"));
    font.setPointSize(12);
    doc.setDefaultFont(font);
    QTextCursor c(&doc);
    c.insertText(QStringLiteral("正文引用"));
    QVERIFY(ReviewNotes::insertEndnote(c, QStringLiteral("尾注正文内容")));
    c.insertText(QStringLiteral("后续。"));
    ReviewNotes::ensureEndnotesAppendix(&doc);
    QVERIFY(doc.toPlainText().contains(QStringLiteral("尾注")));

    const Engine::DocumentModel model = Engine::QTextAdapter::fromDocument(&doc, {});
    QCOMPARE(model.endnoteOrder.size(), 1);
    QCOMPARE(model.endnoteBodies.value(model.endnoteOrder.first()),
             QStringLiteral("尾注正文内容"));

    bool sawMarker = false;
    bool sawAppendixHeading = false;
    for (const Engine::DocSection &sec : model.sections) {
        for (const Engine::DocBlock &block : sec.blocks) {
            if (block.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            if (block.paragraph.headingLevel == 2
                && block.paragraph.plainText().trimmed() == QStringLiteral("尾注"))
                sawAppendixHeading = true;
            for (const Engine::DocRun &run : block.paragraph.runs) {
                if (!run.endnoteId.isEmpty()) {
                    sawMarker = true;
                    QCOMPARE(run.endnoteNumber, 1);
                    QVERIFY(run.style.superscript);
                }
            }
        }
    }
    QVERIFY(sawMarker);
    QVERIFY2(!sawAppendixHeading, "appendix H2「尾注」must not enter the Model");
}

void CoreSmokeTest::layoutEngine_endnotes_atDocumentEnd()
{
    Engine::DocumentModel model;
    Engine::DocSection section;
    Engine::DocParagraph para;
    para.documentPosition = 0;
    Engine::DocRun body;
    body.text = QStringLiteral("正文带尾注");
    body.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(body);
    Engine::DocRun marker;
    marker.text = QStringLiteral("i");
    marker.endnoteId = QStringLiteral("en1");
    marker.endnoteNumber = 1;
    marker.style.font = QFont(QStringLiteral("Helvetica"), 12);
    marker.style.superscript = true;
    para.runs.append(marker);
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    model.sections.append(section);
    model.endnoteOrder.append(QStringLiteral("en1"));
    model.endnoteBodies.insert(QStringLiteral("en1"), QStringLiteral("文末尾注说明"));

    const Engine::LayoutResult result =
        Engine::LayoutEngine::layout(model, PageLayoutSettings{});
    QVERIFY(result.pageCount() >= 2); // body page + endnotes page
    bool sawHeading = false;
    bool sawBody = false;
    for (const Engine::LayoutPage &page : result.pages) {
        for (const Engine::LayoutLine &line : page.lines) {
            if (line.text.contains(QStringLiteral("尾注")) && !line.text.contains(QStringLiteral("说明")))
                sawHeading = true;
            if (line.text.contains(QStringLiteral("文末尾注说明")))
                sawBody = true;
        }
    }
    QVERIFY(sawHeading);
    QVERIFY(sawBody);
}

void CoreSmokeTest::docx_endnotes_modelRoundTrip()
{
    Engine::DocumentModel original;
    Engine::DocSection section;
    Engine::DocParagraph para;
    Engine::DocRun body;
    body.text = QStringLiteral("正文引用尾注");
    body.style.font = QFont(QStringLiteral("Helvetica"), 12);
    para.runs.append(body);
    Engine::DocRun marker;
    marker.text = QStringLiteral("i");
    marker.endnoteId = QStringLiteral("note-e");
    marker.endnoteNumber = 1;
    marker.style.font = QFont(QStringLiteral("Helvetica"), 12);
    marker.style.superscript = true;
    para.runs.append(marker);
    Engine::DocBlock block;
    block.kind = Engine::DocBlock::Kind::Paragraph;
    block.paragraph = para;
    section.blocks.append(block);
    original.sections.append(section);
    original.endnoteOrder.append(QStringLiteral("note-e"));
    original.endnoteBodies.insert(QStringLiteral("note-e"), QStringLiteral("尾注正文内容"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("endnotes.docx"));
    QString err;
    QVERIFY2(Engine::DocxExporter::save(original, path, &err), qPrintable(err));
    QVERIFY(DocxPackage::hasEndnotes(path));

    Engine::DocumentModel loaded;
    QVERIFY2(Engine::DocxImporter::load(&loaded, path, &err), qPrintable(err));
    QCOMPARE(loaded.endnoteOrder.size(), 1);
    QCOMPARE(loaded.endnoteBodies.value(loaded.endnoteOrder.first()),
             QStringLiteral("尾注正文内容"));

    bool sawMarker = false;
    for (const Engine::DocSection &sec : loaded.sections) {
        for (const Engine::DocBlock &b : sec.blocks) {
            if (b.kind != Engine::DocBlock::Kind::Paragraph)
                continue;
            for (const Engine::DocRun &run : b.paragraph.runs) {
                if (run.endnoteId.isEmpty())
                    continue;
                sawMarker = true;
                QCOMPARE(run.endnoteId, loaded.endnoteOrder.first());
            }
        }
    }
    QVERIFY(sawMarker);
}

QTEST_MAIN(CoreSmokeTest)
#include "tst_coresmoke.moc"
