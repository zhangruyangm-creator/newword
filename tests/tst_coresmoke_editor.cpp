#include "coresmoke.h"

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

void CoreSmokeTest::pagedEditorWidget_floatingTextBoxes()
{
    PageLayoutSettings layout;
    QTextDocument doc;
    PagedEditorWidget view(&doc, layout, HeaderFooterSettings{});
    view.resize(1100, 1500);
    view.setHtml(QStringLiteral("<p>第一段</p><p>第二段</p>"));
    view.show();

    FloatingTextBox box = FloatingTextBoxes::makeDefault(0);
    box.xPt = 60;
    box.yPt = 60;
    box.wPt = 200;
    box.hPt = 100;
    box.html = QStringLiteral("<p>浮动文字</p>");
    view.insertFloatingTextBox(box);
    QCOMPARE(FloatingTextBoxes::load(&doc).size(), 1);

    // The box must be visible in the rendered page (gray border/text pixels).
    QImage img(view.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    view.render(&img);
    constexpr qreal k = 96.0 / 72.0;
    const QPoint boxTopLeft(153 + 96 + qRound(box.xPt * k),
                            24 + 141 + qRound(box.yPt * k));
    const QSize boxSize(qRound(box.wPt * k), qRound(box.hPt * k));
    int darkPixels = 0;
    for (int y = boxTopLeft.y(); y < boxTopLeft.y() + boxSize.height(); y += 2) {
        for (int x = boxTopLeft.x(); x < boxTopLeft.x() + boxSize.width(); x += 2) {
            if (qGray(img.pixel(x, y)) < 230)
                ++darkPixels;
        }
    }
    QVERIFY2(darkPixels > 20,
             qPrintable(QStringLiteral("box pixels=%1").arg(darkPixels)));

    // Drag the box: coordinates must change and persist into the document.
    const QPoint center = boxTopLeft + QPoint(boxSize.width() / 2, boxSize.height() / 2);
    QMouseEvent press(QEvent::MouseButtonPress, center, center, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    const QPoint moved = center + QPoint(40, 25);
    QMouseEvent moveEv(QEvent::MouseMove, moved, moved, Qt::NoButton, Qt::LeftButton,
                       Qt::NoModifier);
    QApplication::sendEvent(&view, &moveEv);
    QMouseEvent release(QEvent::MouseButtonRelease, moved, moved, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &release);
    const QVector<FloatingTextBox> afterDrag = FloatingTextBoxes::load(&doc);
    QCOMPARE(afterDrag.size(), 1);
    QVERIFY2(afterDrag.first().xPt > box.xPt,
             qPrintable(QStringLiteral("xPt=%1").arg(afterDrag.first().xPt)));
    QVERIFY2(afterDrag.first().yPt > box.yPt,
             qPrintable(QStringLiteral("yPt=%1").arg(afterDrag.first().yPt)));

    // Double-click opens the inline editor; Escape cancels without changes.
    QMouseEvent dbl(QEvent::MouseButtonDblClick, moved, moved, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &dbl);
    QTextEdit *editor = view.findChild<QTextEdit *>();
    QVERIFY(editor != nullptr);
    QVERIFY(editor->isVisible());
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(editor, &esc);
    QVERIFY(!editor->isVisible());
    QCOMPARE(FloatingTextBoxes::load(&doc).first().html,
             QStringLiteral("<p>浮动文字</p>"));
}

void CoreSmokeTest::pagedEditorWidget_gridAndColumnResize()
{
    PageLayoutSettings layout;

    // --- Grid lines: light 5mm grid must appear and disappear with the toggle.
    QTextDocument doc;
    PagedEditorWidget view(&doc, layout, HeaderFooterSettings{});
    view.resize(1100, 1500);
    view.show();
    auto countGridPixels = [&view]() {
        QImage img(view.size(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        view.render(&img);
        int n = 0;
        for (int y = 180; y < 990; ++y) {
            for (int x = 264; x < 836; ++x) {
                const int g = qGray(img.pixel(x, y));
                if (g > 205 && g < 245)
                    ++n;
            }
        }
        return n;
    };
    view.setGridLinesVisible(true);
    QVERIFY2(countGridPixels() > 200, "grid lines should be visible");
    view.setGridLinesVisible(false);
    QVERIFY2(countGridPixels() < 50, "grid lines should disappear");

    // --- Table column drag-resize changes and persists column widths.
    QTextDocument tableDoc;
    QTextCursor tc(&tableDoc);
    QTextTable *table = tc.insertTable(2, 3);
    for (int r = 0; r < 2; ++r) {
        for (int col = 0; col < 3; ++col) {
            QTextCursor cell(table->cellAt(r, col).firstCursorPosition());
            cell.insertText(QStringLiteral("内容"));
        }
    }
    PagedEditorWidget view2(&tableDoc, layout, HeaderFooterSettings{});
    view2.resize(1100, 1500);
    const QVector<qreal> before = TableGeometry::columnWidthPercents(table);
    const QRectF tableRect = tableDoc.documentLayout()->frameBoundingRect(table);
    const QVector<qreal> edges = TableGeometry::columnEdgeXs(table, tableRect);
    QVERIFY(edges.size() >= 3);
    const QPoint borderPos(153 + 96 + qRound(edges.at(1)),
                           24 + 141 + qRound(tableRect.top()) + 30);
    QMouseEvent press(QEvent::MouseButtonPress, borderPos, borderPos, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view2, &press);
    const QPoint moved = borderPos + QPoint(60, 0);
    QMouseEvent moveEv(QEvent::MouseMove, moved, moved, Qt::NoButton, Qt::LeftButton,
                       Qt::NoModifier);
    QApplication::sendEvent(&view2, &moveEv);
    QMouseEvent release(QEvent::MouseButtonRelease, moved, moved, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view2, &release);
    const QVector<qreal> after = TableGeometry::columnWidthPercents(table);
    QVERIFY2(qAbs(after.at(0) - before.at(0)) > 1.0,
             qPrintable(QStringLiteral("before=%1 after=%2")
                            .arg(before.at(0)).arg(after.at(0))));
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
    // Committed text must NOT inherit the IME preedit underline.
    const int committedPos = text.indexOf(QStringLiteral("你"));
    QVERIFY(committedPos >= 0);
    QTextCursor formatCursor(&doc);
    formatCursor.setPosition(committedPos + 1);
    QVERIFY2(formatCursor.charFormat().underlineStyle() == QTextCharFormat::NoUnderline,
             qPrintable(QStringLiteral("underlineStyle=%1")
                            .arg(int(formatCursor.charFormat().underlineStyle()))));

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
