#include "coresmoke.h"

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
