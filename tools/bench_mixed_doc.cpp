#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QElapsedTimer>
#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextTable>

#include <cstdio>

// 构造带图片 + 表格 + 混排格式的文档（模拟真实报告/论文场景）
static QString paragraphText(int i)
{
    return QStringLiteral("这是第 %1 段混合文档文本，包含 English mix、标点符号与数字 12345，"
                          "用于模拟真实论文与报告的格式负载。段落内会混合粗体斜体颜色字号，"
                          "以制造大量格式碎片（fragment）。")
        .arg(i);
}

static void insertImage(QTextCursor &cur, int seed)
{
    QImage img(180, 110, QImage::Format_RGB32);
    const QRgb c = qRgb(40 + (seed * 37) % 180, 60 + (seed * 53) % 160, 90 + (seed * 71) % 140);
    img.fill(c);
    cur.insertImage(img);
    cur.insertText(QStringLiteral(" "));
}

static void insertTable(QTextCursor &cur, int seed)
{
    QTextTable *t = cur.insertTable(4, 5);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 5; ++c) {
            QTextCursor cell(t->cellAt(r, c).firstCursorPosition());
            cell.insertText(QStringLiteral("T%1%2 单元格文本").arg(seed).arg(r * 5 + c));
        }
    }
    cur.movePosition(QTextCursor::End);
    cur.insertText(QStringLiteral("\n"));
}

static QTextCharFormat randomFormat(int seed)
{
    QTextCharFormat f;
    if (seed % 3 == 0) f.setFontWeight(QFont::Bold);
    if (seed % 4 == 0) f.setFontItalic(true);
    if (seed % 5 == 0) f.setFontUnderline(true);
    if (seed % 6 == 0) f.setForeground(QColor(30 + seed * 23 % 200, 20 + seed * 17 % 200, 60 + seed * 31 % 180));
    if (seed % 7 == 0) f.setFontPointSize(12.0 + (seed % 5));
    return f;
}

static int countFragments(const QTextDocument &d)
{
    int n = 0;
    for (QTextBlock b = d.begin(); b.isValid(); b = b.next())
        for (auto it = b.begin(); !it.atEnd(); ++it)
            ++n;
    return n;
}

static void benchMixed(int chars, int imagesPerK, int tablesPerK)
{
    // 构建：约 chars 字符，每千字符 imagesPerK 张图、tablesPerK 个表，随机格式碎片
    QTextDocument doc;
    QTextCursor cur(&doc);
    cur.beginEditBlock();
    int total = 0;
    int images = 0;
    int tables = 0;
    int para = 0;
    while (total < chars) {
        const QString text = paragraphText(para++);
        // 段内按词混排格式，制造大量 fragment
        const QStringList words = text.split(QLatin1Char(' '));
        for (const QString &w : words) {
            cur.setCharFormat(randomFormat(total + para));
            cur.insertText(w + QStringLiteral(" "));
            total += w.size() + 1;
        }
        cur.insertText(QStringLiteral("\n"));
        ++total;
        if (total / 1000 > images && imagesPerK > 0) { insertImage(cur, images++); }
        if (total / 1000 > tables && tablesPerK > 0) { insertTable(cur, tables++); }
    }
    cur.endEditBlock();

    const int frags = countFragments(doc);
    const int blocks = doc.blockCount();
    std::printf("chars=%-7d blocks=%-5d fragments=%-6d images=%-3d tables=%-3d\n",
                chars, blocks, frags, images, tables);

    // 首次分页/全量布局
    doc.setPageSize(QSizeF(602, 820));
    QElapsedTimer t;
    t.start();
    (void)doc.documentLayout()->documentSize(); // force full layout
    const qint64 fullLayoutMs = t.elapsed();

    // 尾部逐键
    QTextCursor endCur(&doc);
    endCur.movePosition(QTextCursor::End);
    t.restart();
    const int kRounds = 30;
    for (int i = 0; i < kRounds; ++i)
        endCur.insertText(QStringLiteral("字"));
    const qreal appendMs = qreal(t.elapsed()) / kRounds;

    // 中部逐键（避开表格内部，选纯文本区）
    QTextCursor midCur(&doc);
    midCur.setPosition(doc.characterCount() / 2);
    t.restart();
    for (int i = 0; i < kRounds; ++i)
        midCur.insertText(QStringLiteral("字"));
    const qreal middleMs = qreal(t.elapsed()) / kRounds;

    // 表格内逐键
    QTextCursor tblCur(&doc);
    QTextTable *tbl = nullptr;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next())
        if (QTextTable *tt = qobject_cast<QTextTable *>(QTextCursor(b).currentTable())) { tbl = tt; break; }
    qreal tableMs = -1.0;
    if (tbl) {
        tblCur = QTextCursor(tbl->cellAt(2, 2).firstCursorPosition());
        tblCur.movePosition(QTextCursor::EndOfBlock);
        t.restart();
        for (int i = 0; i < kRounds; ++i)
            tblCur.insertText(QStringLiteral("字"));
        tableMs = qreal(t.elapsed()) / kRounds;
    }

    // 图片密集区逐键（最后一张图所在块末尾）
    QTextCursor imgCur(&doc);
    QTextBlock lastImgBlock;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        QTextCursor bc(b);
        if (bc.blockFormat().hasProperty(QTextFormat::ImageName)) lastImgBlock = b;
    }
    qreal imgMs = -1.0;
    if (lastImgBlock.isValid()) {
        imgCur = QTextCursor(lastImgBlock);
        imgCur.movePosition(QTextCursor::EndOfBlock);
        t.restart();
        for (int i = 0; i < kRounds; ++i)
            imgCur.insertText(QStringLiteral("字"));
        imgMs = qreal(t.elapsed()) / kRounds;
    }

    std::printf("fullLayout=%-6lldms appendKey=%.2fms middleKey=%.2fms tableKey=%.2fms imgKey=%.2fms\n",
                fullLayoutMs, appendMs, middleMs, tableMs, imgMs);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    std::printf("--- Mixed doc (images + tables + fragmented format) ---\n");
    benchMixed(50'000, 0, 0);   // 纯文本对照
    benchMixed(50'000, 8, 2);   // 50k 字符：约 400 图 + 100 表
    benchMixed(200'000, 8, 2);  // 200k 字符（≈190 页）：约 1600 图 + 400 表
    benchMixed(200'000, 20, 5); // 200k 字符、图片表格更密集
    return 0;
}
