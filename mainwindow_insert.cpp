#include "mainwindow.h"
#include "documenttab.h"
#include "pagededitorwidget.h"
#include "formuladialog.h"
#include "formulaio.h"
#include "formularenderer.h"
#include "documentsections.h"
#include "imageprops.h"
#include "imagepropertiesdialog.h"
#include "reviewnotes.h"

#include <QColorDialog>
#include <QDate>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QColor>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QTextTable>
#include <QTextTableCell>
#include <QTextTableFormat>
#include <QTime>
#include <QUrl>
#include <QVariant>

#include "tablegeometry.h"

#include <optional>

void MainWindow::insertImage()
{
    DocumentTab *tab = currentTab();
    auto *editor = currentEditor();
    if (!tab || !editor)
        return;
    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("插入图片"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;所有文件 (*)"));
    if (fileName.isEmpty())
        return;
    tab->insertImageFile(fileName); // uses DocumentTab path (block + fit)
}

void MainWindow::editImageProperties()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt = cursor.charFormat();
    if (!fmt.isImageFormat()) {
        // Try selection / nearby fragment
        QTextBlock block = cursor.block();
        bool found = false;
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            if (frag.charFormat().isImageFormat()) {
                cursor.setPosition(frag.position());
                cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                fmt = frag.charFormat();
                found = true;
                break;
            }
        }
        if (!found) {
            QMessageBox::information(this, tr("图片属性"), tr("请先选中或点击一张图片。"));
            return;
        }
    }
    QTextImageFormat imageFmt = fmt.toImageFormat();
    QImage image;
    const QVariant res = editor->document()->resource(QTextDocument::ImageResource,
                                                      QUrl(imageFmt.name()));
    if (res.canConvert<QImage>())
        image = res.value<QImage>();

    ImagePropertiesDialog dialog(imageFmt, image, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QTextImageFormat updated = dialog.format();
    cursor.mergeCharFormat(updated);
    // Alignment for block images
    QTextBlockFormat bf = cursor.blockFormat();
    bf.setAlignment(ImageProps::alignOf(updated));
    cursor.mergeBlockFormat(bf);
    editor->setTextCursor(cursor);
}

namespace {
[[nodiscard]] std::optional<QString> selectFormulaImageAtCursor(QTextCursor *cursor)
{
    if (!cursor || !cursor->document())
        return std::nullopt;

    auto tryFragment = [&](const QTextFragment &frag) -> std::optional<QString> {
        if (!frag.isValid() || !frag.charFormat().isImageFormat())
            return std::nullopt;
        const QString name = frag.charFormat().toImageFormat().name();
        if (!FormulaRenderer::isFormulaResource(name))
            return std::nullopt;
        cursor->setPosition(frag.position());
        cursor->setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
        return FormulaRenderer::latexFromResourceName(name);
    };

    const int pos = cursor->position();
    QTextBlock block = cursor->block();
    for (auto it = block.begin(); !(it.atEnd()); ++it) {
        const QTextFragment frag = it.fragment();
        const int start = frag.position();
        const int end = start + frag.length();
        if (pos >= start && pos <= end) {
            if (auto latex = tryFragment(frag))
                return latex;
        }
    }

    if (pos > 0) {
        QTextCursor left(*cursor);
        left.setPosition(pos - 1);
        block = left.block();
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            const int start = frag.position();
            const int end = start + frag.length();
            if (pos - 1 >= start && pos - 1 < end) {
                if (auto latex = tryFragment(frag))
                    return latex;
            }
        }
    }
    return std::nullopt;
}

void insertFormulaImage(PagedEditorWidget *editor, const QString &latex, qreal pointSize,
                        bool replace)
{
    if (!editor || latex.trimmed().isEmpty())
        return;

    const qreal dpr = qMax<qreal>(1.0, editor->devicePixelRatioF());
    QImage image = FormulaRenderer::render(latex, pointSize, dpr);
    if (image.isNull())
        return;

    const QString name = FormulaRenderer::resourceNameForLatex(latex, pointSize);
    editor->document()->addResource(QTextDocument::ImageResource, QUrl(name), QVariant(image));

    QTextImageFormat imageFormat;
    imageFormat.setName(name);
    imageFormat.setWidth(image.width() / dpr);
    imageFormat.setHeight(image.height() / dpr);
    // Center formulas on the text line (Qt default is bottom-on-baseline,
    // which makes tall formulas stick up above the text).
    imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    imageFormat.setProperty(FormulaIO::PointSizeProperty, pointSize);
    imageFormat.setProperty(QTextFormat::ImageAltText, FormulaIO::makeLatexAlt(latex));

    QTextCursor cursor = editor->textCursor();
    if (replace && !selectFormulaImageAtCursor(&cursor))
        replace = false;
    cursor.insertImage(imageFormat);
    editor->setTextCursor(cursor);
}
} // namespace

void MainWindow::insertFormula()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    qreal pointSize = 18.0;
    QTextCursor cursor = editor->textCursor();
    const auto existing = selectFormulaImageAtCursor(&cursor);
    const bool replacing = existing.has_value();
    if (replacing) {
        editor->setTextCursor(cursor);
        const QTextImageFormat img = cursor.charFormat().toImageFormat();
        if (img.hasProperty(FormulaIO::PointSizeProperty))
            pointSize = img.doubleProperty(FormulaIO::PointSizeProperty);
    }

    FormulaDialog dialog(existing.value_or(QString()), pointSize, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    insertFormulaImage(editor, dialog.latex(), dialog.pointSize(), replacing);
}

void MainWindow::editFormula(const QString &latex, int documentPosition)
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    QTextCursor cursor = editor->textCursor();
    qreal pointSize = 18.0;
    bool replacing = false;

    if (documentPosition >= 0) {
        QTextBlock block = editor->document()->findBlock(documentPosition);
        for (auto it = block.begin(); !(it.atEnd()); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || frag.position() != documentPosition)
                continue;
            if (!frag.charFormat().isImageFormat())
                break;
            const QTextImageFormat img = frag.charFormat().toImageFormat();
            if (!FormulaRenderer::isFormulaResource(img.name()))
                break;
            cursor.setPosition(frag.position());
            cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            if (img.hasProperty(FormulaIO::PointSizeProperty))
                pointSize = img.doubleProperty(FormulaIO::PointSizeProperty);
            replacing = true;
            break;
        }
    }

    if (!replacing) {
        replacing = selectFormulaImageAtCursor(&cursor).has_value();
        if (replacing) {
            const QTextImageFormat img = cursor.charFormat().toImageFormat();
            if (img.hasProperty(FormulaIO::PointSizeProperty))
                pointSize = img.doubleProperty(FormulaIO::PointSizeProperty);
        } else {
            QTextDocument *doc = editor->document();
            for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
                for (auto it = block.begin(); !(it.atEnd()); ++it) {
                    const QTextFragment frag = it.fragment();
                    if (!frag.isValid() || !frag.charFormat().isImageFormat())
                        continue;
                    const QTextImageFormat img = frag.charFormat().toImageFormat();
                    if (FormulaRenderer::latexFromResourceName(img.name()) != latex)
                        continue;
                    cursor.setPosition(frag.position());
                    cursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                    if (img.hasProperty(FormulaIO::PointSizeProperty))
                        pointSize = img.doubleProperty(FormulaIO::PointSizeProperty);
                    replacing = true;
                    break;
                }
                if (replacing)
                    break;
            }
        }
    }

    if (replacing)
        editor->setTextCursor(cursor);

    FormulaDialog dialog(latex, pointSize, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    insertFormulaImage(editor, dialog.latex(), dialog.pointSize(), replacing);
}

void MainWindow::insertTextBox()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    tab->insertFloatingTextBox();
    statusBar()->showMessage(
        tr("已插入浮动文本框：拖动可移动，双击编辑文字，Esc 结束编辑，右下角可调大小"), 5000);
}

void MainWindow::insertHorizontalLine()
{
    if (auto *e = currentEditor())
        e->textCursor().insertHtml(QStringLiteral("<hr/><p></p>"));
}

void MainWindow::insertPageBreak()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextBlockFormat fmt;
    fmt.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    editor->textCursor().insertBlock(fmt);
}

void MainWindow::insertSectionBreak()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    DocumentSections::insertSectionBreak(cursor);
}

void MainWindow::insertFootnote()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    (void)ReviewNotes::promptAndInsertFootnote(this, cursor);
}

void MainWindow::insertEndnote()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    (void)ReviewNotes::promptAndInsertEndnote(this, cursor);
}

void MainWindow::insertComment()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        QMessageBox::information(this, tr("批注"), tr("请先选中要批注的文本。"));
        return;
    }
    (void)ReviewNotes::promptAndInsertComment(this, cursor);
}

void MainWindow::showComments()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    const auto comments = ReviewNotes::collectComments(editor->document());
    if (comments.isEmpty()) {
        QMessageBox::information(this, tr("批注"), tr("当前文档没有批注。"));
        return;
    }
    QString text;
    int i = 1;
    for (const ReviewNotes::Comment &c : comments) {
        text += tr("%1. [%2] %3\n").arg(i).arg(c.author, c.text);
        ++i;
    }
    QMessageBox::information(this, tr("批注列表"), text);
}

void MainWindow::insertDate()
{
    if (auto *e = currentEditor())
        e->textCursor().insertText(QDate::currentDate().toString(QStringLiteral("yyyy年M月d日")));
}

void MainWindow::insertTime()
{
    if (auto *e = currentEditor())
        e->textCursor().insertText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
}

void MainWindow::insertLink()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    QString selected = cursor.selectedText();
    bool ok = false;
    const QString url = QInputDialog::getText(this, tr("插入超链接"), tr("网址:"),
                                              QLineEdit::Normal, QStringLiteral("https://"), &ok);
    if (!ok || url.trimmed().isEmpty())
        return;
    QString text = selected;
    if (text.isEmpty()) {
        text = QInputDialog::getText(this, tr("插入超链接"), tr("显示文字:"),
                                     QLineEdit::Normal, url, &ok);
        if (!ok || text.isEmpty())
            text = url;
    }
    QTextCharFormat linkFormat;
    linkFormat.setAnchor(true);
    linkFormat.setAnchorHref(url);
    linkFormat.setForeground(QColor(QStringLiteral("#0563C1")));
    linkFormat.setFontUnderline(true);
    cursor.insertText(text, linkFormat);
}

void MainWindow::insertTableOfContents()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QStringList entries;
    for (QTextBlock block = editor->document()->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0)
            continue;
        const QString title = block.text().trimmed();
        if (!title.isEmpty())
            entries << QStringLiteral("%1%2").arg(QString(level * 2, QLatin1Char(' ')), title);
    }
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("目录"), tr("未找到标题。请先使用标题样式。"));
        return;
    }
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.insertHtml(QStringLiteral("<h2>%1</h2>").arg(tr("目录")));
    for (const QString &line : entries)
        cursor.insertText(line + QLatin1Char('\n'));
    cursor.insertBlock();
    cursor.endEditBlock();
}
