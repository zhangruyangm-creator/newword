#include "headerfootersettings.h"
#include "pagegeometry.h"
#include "pagededitorwidget.h"
#include "pagelayout.h"

#include <QApplication>
#include <QImage>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include <cstdio>

// Offscreen visual QA: renders the self-drawn paged editor to a PNG.
int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    PageLayoutSettings layout;
    HeaderFooterSettings headerFooter;
    headerFooter.header = QStringLiteral("NewWord 分页原型 · 页眉");
    headerFooter.footer = QStringLiteral("测试页脚");

    QTextDocument doc;
    doc.setUndoRedoEnabled(true);
    {
        QTextCursor c(&doc);
        for (int i = 0; i < 120; ++i) {
            c.insertText(QStringLiteral(
                "这是一段用于验证真实分页的示例文字，包含中文与 English mix，"
                "验证文字不会穿过页眉页脚区域。"));
            c.insertBlock();
        }
        c.insertText(QStringLiteral("结尾。"));
    }

    PagedEditorWidget view(&doc, layout, headerFooter);
    view.resize(1100, 1600);
    view.show();

    QTimer::singleShot(150, [&]() {
        QImage img(view.size(), QImage::Format_ARGB32);
        view.render(&img);
        const QString out = QStringLiteral("/tmp/paged_editor_preview.png");
        if (img.save(out))
            std::printf("saved %s (%dx%d) pages=%d\n", qPrintable(out), img.width(),
                        img.height(), view.pageCount());
        else
            std::fprintf(stderr, "failed to save %s\n", qPrintable(out));
        app.quit();
    });

    return app.exec();
}
