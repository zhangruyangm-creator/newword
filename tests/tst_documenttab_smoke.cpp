#include "documenttab.h"
#include "formularenderer.h"
#include "formulaio.h"
#include "pagegeometry.h"
#include "pagededitorwidget.h"
#include "webpimage.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QTextLayout>
#include <QTextLine>
#include <QTextBlock>
#include <QTextFragment>
#include <QtTest/QtTest>

class DocumentTabSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void viewSwitchAndTyping();
    void widgetSetHtmlKeepsParagraphs();
    void webpLoads();
    void formulaInsertAlignAndDoubleClick();
};

void DocumentTabSmokeTest::viewSwitchAndTyping()
{
    DocumentTab tab;
    tab.resize(1100, 900);
    PagedEditorWidget *editor = tab.editor();
    QVERIFY(editor != nullptr);

    editor->setHtml(QStringLiteral("<p>第一段内容，用于分页编辑器冒烟测试。</p>"
                                   "<p>第二段内容。</p>"));
    tab.applyEditorDefaults();
    QCOMPARE(tab.viewMode(), DocumentViewMode::Page);
    QVERIFY(tab.pageCount() >= 1);

    QKeyEvent keyA(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("A"));
    QApplication::sendEvent(editor, &keyA);
    QVERIFY(editor->toPlainText().contains(QLatin1Char('A')));

    // View switching must keep the same document and stay stable.
    tab.setViewMode(DocumentViewMode::Draft);
    QCOMPARE(tab.viewMode(), DocumentViewMode::Draft);
    tab.setViewMode(DocumentViewMode::Page);
    tab.setViewMode(DocumentViewMode::Reading);
    tab.setViewMode(DocumentViewMode::Outline);
    tab.setViewMode(DocumentViewMode::Page);
    QVERIFY(tab.pageCount() >= 1);
    QVERIFY(editor->document()->toPlainText().contains(QLatin1Char('A')));

    // Ruler alignment: the page must be centered, not stuck to the left edge.
    editor->resize(1100, 800);
    const int pageW = PageGeometry::from(PageLayoutSettings{}, 100).pageWidthPx;
    QCOMPARE(editor->pageOffsetX(), (1100 - pageW) / 2);

    // The page view must actually paint.
    tab.show();
    QVERIFY(editor->isVisible());
    QImage img(editor->size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    editor->render(&img);
    bool hasContent = false;
    for (int y = 0; y < img.height(); y += 3) {
        for (int x = 0; x < img.width(); x += 3) {
            if (qGray(img.pixel(x, y)) < 200) {
                hasContent = true;
                break;
            }
        }
        if (hasContent)
            break;
    }
    QVERIFY2(hasContent, "page view should render non-blank content");
}

void DocumentTabSmokeTest::widgetSetHtmlKeepsParagraphs()
{
    QTextDocument doc;
    PagedEditorWidget view(&doc, PageLayoutSettings{}, HeaderFooterSettings{});
    view.setHtml(QStringLiteral("<p>第一段内容。</p><p>第二段内容。</p>"));
    QCOMPARE(view.toPlainText(), QStringLiteral("第一段内容。\n第二段内容。"));

    QTextDocument plain;
    plain.setHtml(QStringLiteral("<p>第一段内容。</p><p>第二段内容。</p>"));
    QCOMPARE(plain.toPlainText(), QStringLiteral("第一段内容。\n第二段内容。"));
}

void DocumentTabSmokeTest::webpLoads()
{
    const QString path = QStringLiteral("/tmp/test.webp");
    if (!QFileInfo::exists(path))
        QSKIP("no sample webp at /tmp/test.webp");
    const QImage image = loadWebpImage(path);
    QVERIFY2(!image.isNull(), "webp must decode via ImageIO");
    QCOMPARE(image.width(), 64);
    QCOMPARE(image.height(), 48);
    // Spot check a known pixel: row 1 col 1 was (255, 80, 80, 255) before encode.
    const QColor c = image.pixelColor(1, 1);
    QVERIFY2(qAbs(c.red() - 255) < 40 && qAbs(c.green() - 80) < 40,
             qPrintable(QStringLiteral("red=%1 green=%2").arg(c.red()).arg(c.green())));
}

void DocumentTabSmokeTest::formulaInsertAlignAndDoubleClick()
{
    PageLayoutSettings layout;
    QTextDocument doc;
    PagedEditorWidget view(&doc, layout, HeaderFooterSettings{});
    view.resize(1100, 1500);
    view.setHtml(QStringLiteral("<p>e=mc^2 示例文字</p>"));
    view.show();

    QTextCursor c = view.textCursor();
    c.movePosition(QTextCursor::End);
    view.setTextCursor(c);

    const qreal dpr = 2.0;
    const QString latex = QStringLiteral("e=mc^2");
    const QImage image = FormulaRenderer::render(latex, 18.0, dpr);
    QVERIFY(!image.isNull());
    const QString name = FormulaRenderer::resourceNameForLatex(latex, 18.0);
    doc.addResource(QTextDocument::ImageResource, QUrl(name), image);
    QTextImageFormat fmt;
    fmt.setName(name);
    fmt.setWidth(image.width() / dpr);
    fmt.setHeight(image.height() / dpr);
    fmt.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    fmt.setProperty(FormulaIO::PointSizeProperty, 18.0);
    QTextCursor ic = view.textCursor();
    ic.insertImage(fmt);
    view.setTextCursor(ic);

    int imgPos = -1;
    QTextCharFormat imgCharFormat;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment f = it.fragment();
            if (f.charFormat().isImageFormat()
                && FormulaRenderer::isFormulaResource(
                    f.charFormat().toImageFormat().name())) {
                imgPos = f.position();
                imgCharFormat = f.charFormat();
                break;
            }
        }
        if (imgPos >= 0)
            break;
    }
    QVERIFY(imgPos >= 0);
    QVERIFY(imgCharFormat.verticalAlignment() == QTextCharFormat::AlignMiddle);

    const QTextBlock block = doc.findBlock(imgPos);
    const QRectF br = doc.documentLayout()->blockBoundingRect(block);
    const QTextLayout *tl = block.layout();
    const QTextLine line = tl->lineForTextPosition(imgPos - block.position());
    QVERIFY(line.isValid());
    const qreal imgLeft = line.cursorToX(imgPos);
    const qreal imgRight = line.cursorToX(imgPos + 1);
    const QPoint center(249 + qRound((imgLeft + imgRight) / 2.0),
                        165 + qRound(br.top() + line.y() + line.height() / 2.0));
    const QTextCursor hit = view.cursorForPosition(center);
    QVERIFY2(hit.position() == imgPos || hit.position() == imgPos + 1,
             qPrintable(QStringLiteral("hitPos=%1 imgPos=%2")
                            .arg(hit.position()).arg(imgPos)));

    QImage out(view.size(), QImage::Format_ARGB32);
    out.fill(Qt::white);
    view.render(&out);
    const int y0 = 165 + qRound(br.top() + line.y());
    const int y1 = 165 + qRound(br.top() + line.y() + line.height());
    auto darkCenterY = [&](int x0, int x1) {
        int minY = -1;
        int maxY = -1;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                if (qGray(out.pixel(x, y)) < 120) {
                    if (minY < 0)
                        minY = y;
                    maxY = y;
                    break;
                }
            }
        }
        return (minY + maxY) / 2.0;
    };
    const int textCenter = darkCenterY(249, 249 + qRound(imgLeft) - 4);
    const int formulaCenter = darkCenterY(249 + qRound(imgLeft), 249 + qRound(imgRight));
    // The formula must be vertically centered on the text line (not stuck above).
    QVERIFY2(qAbs(formulaCenter - textCenter) < 10,
             qPrintable(QStringLiteral("textCenter=%1 formulaCenter=%2")
                            .arg(textCenter).arg(formulaCenter)));

    // Double-clicking the formula requests an edit.
    bool dblClicked = false;
    int dblPos = -1;
    QObject::connect(&view, &PagedEditorWidget::imageDoubleClicked, &view,
                     [&](int p) {
                         dblClicked = true;
                         dblPos = p;
                     });
    QMouseEvent dbl(QEvent::MouseButtonDblClick, center, center, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &dbl);
    QVERIFY(dblClicked);
    QVERIFY(dblPos == imgPos || dblPos == imgPos + 1);
}

QTEST_MAIN(DocumentTabSmokeTest)
#include "tst_documenttab_smoke.moc"
