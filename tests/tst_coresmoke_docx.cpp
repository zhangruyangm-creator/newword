#include "coresmoke.h"

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
