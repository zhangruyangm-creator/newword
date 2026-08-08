#include "documenttab.h"
#include "pagegeometry.h"
#include "pagededitorwidget.h"
#include "webpimage.h"

#include <QApplication>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QtTest/QtTest>

class DocumentTabSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void viewSwitchAndTyping();
    void widgetSetHtmlKeepsParagraphs();
    void webpLoads();
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

QTEST_MAIN(DocumentTabSmokeTest)
#include "tst_documenttab_smoke.moc"
