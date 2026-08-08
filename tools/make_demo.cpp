#include "docxio.h"
#include "styleutils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTextTableFormat>
#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QString outPath = QStringLiteral(NEWWORD_SOURCE_DIR "/examples/demo.docx");
    if (argc > 1)
        outPath = QString::fromLocal8Bit(argv[1]);

    QDir().mkpath(QFileInfo(outPath).absolutePath());

    QTextDocument doc;
    QTextCursor c(&doc);

    QTextBlockFormat h1;
    h1.setHeadingLevel(1);
    c.setBlockFormat(h1);
    QTextCharFormat h1Char;
    h1Char.setFontWeight(QFont::Bold);
    h1Char.setFontPointSize(18);
    c.insertText(QStringLiteral("NewWord 0.5 样例文档"), h1Char);

    c.insertBlock();
    QTextBlockFormat body;
    body.setHeadingLevel(0);
    c.setBlockFormat(body);
    c.setCharFormat(QTextCharFormat());
    c.insertText(QStringLiteral(
        "这是验收用样例：打开 → 修改 → 保存 DOCX → 导出 PDF。"
        "DOCX 为有损互通，不承诺与 Word 完全一致。"));

    c.insertBlock();
    QTextBlockFormat h2;
    h2.setHeadingLevel(2);
    c.setBlockFormat(h2);
    QTextCharFormat h2Char;
    h2Char.setFontWeight(QFont::Bold);
    h2Char.setFontPointSize(14);
    c.insertText(QStringLiteral("表格"), h2Char);

    c.insertBlock();
    c.setBlockFormat(body);
    c.setCharFormat(QTextCharFormat());

    QTextTableFormat tf;
    tf.setBorder(1);
    tf.setCellPadding(4);
    QTextTable *table = c.insertTable(3, 2, tf);
    table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("项目"));
    table->cellAt(0, 1).firstCursorPosition().insertText(QStringLiteral("说明"));
    table->cellAt(1, 0).firstCursorPosition().insertText(QStringLiteral("页面视图"));
    table->cellAt(1, 1).firstCursorPosition().insertText(QStringLiteral("打印预览风格近似"));
    table->cellAt(2, 0).firstCursorPosition().insertText(QStringLiteral("DOCX"));
    table->cellAt(2, 1).firstCursorPosition().insertText(QStringLiteral("桥接优先，内置回退"));

    c.movePosition(QTextCursor::End);
    c.insertBlock();
    c.setBlockFormat(h2);
    c.setCharFormat(h2Char);
    c.insertText(QStringLiteral("下一步"));

    c.insertBlock();
    c.setBlockFormat(body);
    c.setCharFormat(QTextCharFormat());
    c.insertText(QStringLiteral("在 NewWord 中打开本文件，确认标题与表格可见，然后导出 PDF。"));

    QString err;
    if (!DocxIO::save(&doc, outPath, &err)) {
        std::fprintf(stderr, "Failed to write %s: %s\n", qPrintable(outPath), qPrintable(err));
        return 1;
    }
    std::printf("Wrote %s\n", qPrintable(outPath));
    return 0;
}
