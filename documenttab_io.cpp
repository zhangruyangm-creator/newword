#include "documenttab.h"
#include "documentrecovery.h"
#include "docxconverter.h"
#include "docxmeta.h"
#include "docxpackage.h"
#include "floatingtextbox.h"
#include "formulaio.h"
#include "imageprops.h"
#include "outlineviewwidget.h"
#include "pagegeometry.h"
#include "webpimage.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <QTextImageFormat>
#include <QTextStream>
#include <QUrl>
#include <QVector>

void DocumentTab::insertImageFile(const QString &fileName)
{
    QImage image(fileName);
    if (image.isNull()
        && QFileInfo(fileName).suffix().compare(QLatin1String("webp"),
                                                Qt::CaseInsensitive)
               == 0) {
        image = loadWebpImage(fileName);
    }
    if (image.isNull())
        return;

    QUrl uri = QUrl::fromLocalFile(fileName);
    QTextImageFormat imageFormat;
    imageFormat.setName(uri.toString());
    const PageGeometry geo = PageGeometry::from(m_pageLayout, m_zoomPercent);
    const int maxW = qMax(200, geo.pageWidthPx - 2 * geo.marginPx);
    ImageProps::fitToMaxWidth(&imageFormat, image, qMin(480, maxW));
    // Store a bounded-resource copy: memory and repaint cost stay proportional
    // to the display size, not the camera resolution.
    const int cap = qMin(2048, 2 * qRound(imageFormat.width()));
    const int longest = qMax(image.width(), image.height());
    if (longest > cap) {
        const qreal scale = qreal(cap) / longest;
        image = image.scaled(qMax(1, int(image.width() * scale)),
                             qMax(1, int(image.height() * scale)), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    m_editor->document()->addResource(QTextDocument::ImageResource, uri, image);
    ImageProps::setWrap(&imageFormat, ImageProps::Wrap::Block);
    ImageProps::setAlign(&imageFormat, Qt::AlignHCenter);

    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();
    if (!cursor.atBlockStart())
        cursor.insertBlock();
    QTextBlockFormat bf = cursor.blockFormat();
    bf.setAlignment(Qt::AlignHCenter);
    cursor.setBlockFormat(bf);
    cursor.insertImage(imageFormat);
    cursor.insertBlock();
    QTextBlockFormat body;
    body.setAlignment(Qt::AlignLeft);
    cursor.setBlockFormat(body);
    cursor.endEditBlock();
    m_editor->setTextCursor(cursor);
}

void DocumentTab::applyEditorDefaults()
{
    EditorDefaults::loadFromSettings();
    m_editor->document()->setDefaultFont(EditorDefaults::documentFont());
    setPageLayout(EditorDefaults::pageLayout());
    setHeaderFooter(EditorDefaults::headerFooter());
    QTextCursor cursor(m_editor->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setFont(EditorDefaults::documentFont());
    cursor.mergeCharFormat(fmt);
    cursor.clearSelection();
    cursor.setPosition(0);
    m_editor->setTextCursor(cursor);
    m_editor->document()->setModified(false);
}

void DocumentTab::loadRecoveryContent(const QString &html,
                                      const HeaderFooterSettings &headerFooter,
                                      const PageLayoutSettings &pageLayout,
                                      const QString &sourcePath)
{
    const qreal dpr = qMax<qreal>(1.0, m_editor->devicePixelRatioF());
    beginBulkDocumentUpdate();
    const QVector<FloatingTextBox> boxes = FloatingTextBoxes::extractFromHtml(html);
    m_editor->setHtml(FloatingTextBoxes::stripMarkerFromHtml(html));
    FormulaIO::restoreFormulasFromHtml(m_editor->document(), dpr);
    FloatingTextBoxes::save(m_editor->document(), boxes, false);
    setHeaderFooter(headerFooter);
    setPageLayout(pageLayout);
    m_filePath = sourcePath;
    m_editor->document()->setModified(true);
    if (m_viewMode == DocumentViewMode::Outline)
        m_outlineView->setDocument(m_editor->document());
    endBulkDocumentUpdate();
}

bool DocumentTab::loadFromFile(const QString &fileName, QString *errorMessage)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    const qreal dpr = qMax<qreal>(1.0, m_editor->devicePixelRatioF());

    beginBulkDocumentUpdate();

    if (suffix == QLatin1String("docx")) {
        DocxDocumentMeta meta;
        if (!DocxConverter::load(m_editor->document(), fileName, errorMessage, &meta)) {
            endBulkDocumentUpdate();
            return false;
        }
        PagedEditorWidget::normalizeDocumentStructure(m_editor->document());
        PagedEditorWidget::downscaleImageResources(m_editor->document());
        m_headerFooter = meta.headerFooter;
        if (meta.pageLayout.pageSizeMm().width() > 10)
            setPageLayout(meta.pageLayout);
        m_editor->setHeaderFooter(m_headerFooter);
        FloatingTextBoxes::save(m_editor->document(), DocxPackage::readFloatingBoxes(fileName),
                                false);
    } else {
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            if (errorMessage)
                *errorMessage = file.errorString();
            endBulkDocumentUpdate();
            return false;
        }

        const QByteArray data = file.readAll();
        const QString text = QString::fromUtf8(data);

        if (suffix == QLatin1String("html") || suffix == QLatin1String("htm")) {
            const QVector<FloatingTextBox> boxes = FloatingTextBoxes::extractFromHtml(text);
            m_editor->setHtml(FloatingTextBoxes::stripMarkerFromHtml(text));
            FormulaIO::restoreFormulasFromHtml(m_editor->document(), dpr);
            FloatingTextBoxes::save(m_editor->document(), boxes, false);
        } else if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")) {
            QStringList formulas;
            const QString processed = FormulaIO::extractMarkdownFormulas(text, &formulas);
            m_editor->document()->setMarkdown(processed, QTextDocument::MarkdownDialectGitHub);
            FormulaIO::injectMarkdownFormulas(m_editor->document(), formulas, dpr);
            PagedEditorWidget::normalizeDocumentStructure(m_editor->document());
            PagedEditorWidget::downscaleImageResources(m_editor->document());
        } else {
            m_editor->setPlainText(text);
        }
    }

    m_filePath = fileName;
    m_editor->document()->setModified(false);
    if (m_viewMode == DocumentViewMode::Outline)
        m_outlineView->setDocument(m_editor->document());
    endBulkDocumentUpdate();
    return true;
}

bool DocumentTab::loadFromPreparedDocx(const DocxConverter::PrepareResult &prepared,
                                       const QString &fileName,
                                       QString *errorMessage)
{
    beginBulkDocumentUpdate();

    DocxDocumentMeta meta = prepared.meta;
    if (prepared.ok && !prepared.html.isEmpty()) {
        const qreal dpr = qMax<qreal>(1.0, m_editor->devicePixelRatioF());
        m_editor->setHtml(prepared.html);
        PagedEditorWidget::normalizeDocumentStructure(m_editor->document());
        PagedEditorWidget::downscaleImageResources(m_editor->document());
        FormulaIO::restoreFormulasFromHtml(m_editor->document(), dpr);
        m_editor->document()->setModified(false);
    } else if (!DocxConverter::applyPrepared(m_editor->document(), prepared, fileName,
                                             errorMessage, &meta)) {
        endBulkDocumentUpdate();
        return false;
    }
    PagedEditorWidget::normalizeDocumentStructure(m_editor->document());
    PagedEditorWidget::downscaleImageResources(m_editor->document());

    m_headerFooter = meta.headerFooter;
    if (meta.pageLayout.pageSizeMm().width() > 10)
        setPageLayout(meta.pageLayout);
    m_editor->setHeaderFooter(m_headerFooter);
    FloatingTextBoxes::save(m_editor->document(), DocxPackage::readFloatingBoxes(fileName), false);
    m_filePath = fileName;
    m_editor->document()->setModified(false);
    if (m_viewMode == DocumentViewMode::Outline)
        m_outlineView->setDocument(m_editor->document());
    endBulkDocumentUpdate();
    return true;
}

bool DocumentTab::saveToFile(const QString &fileName, QString *errorMessage)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();

    if (suffix == QLatin1String("docx")) {
        DocxDocumentMeta meta;
        meta.headerFooter = m_headerFooter;
        meta.pageLayout = m_pageLayout;
        if (!DocxConverter::save(m_editor->document(), fileName, errorMessage, &meta))
            return false;
        if (!DocxPackage::writeFloatingBoxes(fileName, FloatingTextBoxes::load(m_editor->document()),
                                             errorMessage))
            return false;
    } else if (suffix == QLatin1String("odt") || suffix == QLatin1String("odf")) {
        QTextDocumentWriter writer(fileName, "odf");
        if (!writer.write(m_editor->document())) {
            if (errorMessage)
                *errorMessage = tr("无法写入 ODT 文件。");
            return false;
        }
    } else {
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            if (errorMessage)
                *errorMessage = file.errorString();
            return false;
        }
        QTextStream out(&file);
        if (suffix == QLatin1String("txt"))
            out << m_editor->toPlainText();
        else if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown"))
            out << FormulaIO::documentToMarkdownWithFormulas(m_editor->document());
        else
            out << FloatingTextBoxes::embedInHtml(
                FormulaIO::documentToHtmlWithFormulas(m_editor->document()),
                FloatingTextBoxes::load(m_editor->document()));
    }

    m_filePath = fileName;
    m_editor->document()->setModified(false);
    return true;
}
