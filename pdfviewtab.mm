#include "pdfviewtab.h"

#include <QFileInfo>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#import <PDFKit/PDFKit.h>
#import <AppKit/AppKit.h>
#endif

PdfViewTab::PdfViewTab(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral("background: #5a5a5a;"));
}

PdfViewTab::~PdfViewTab()
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        if (m_pdfView) {
            PDFView *view = static_cast<PDFView *>(m_pdfView);
            m_pdfView = nullptr;
            [view removeFromSuperview];
            [view release];
        }
    }
#endif
}

bool PdfViewTab::loadFile(const QString &filePath, QString *errorMessage)
{
#ifdef Q_OS_MACOS
    ensureNativeView();
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        NSURL *url = [NSURL fileURLWithPath:filePath.toNSString()];
        PDFDocument *doc = [[PDFDocument alloc] initWithURL:url];
        if (!doc || doc.pageCount == 0) {
            if (errorMessage)
                *errorMessage = tr("无法打开 PDF 文件。");
            return false;
        }
        view.document = doc;
        view.autoScales = NO;
        view.displayMode = kPDFDisplaySinglePageContinuous;
        view.displayDirection = kPDFDisplayDirectionVertical;
        applyZoom();
        m_filePath = filePath;
        syncPageSignal();
        return true;
    }
#else
    Q_UNUSED(filePath)
    if (errorMessage)
        *errorMessage = tr("当前平台不支持 PDF 阅读。");
    return false;
#endif
}

QString PdfViewTab::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("PDF");
    return QFileInfo(m_filePath).fileName();
}

int PdfViewTab::pageCount() const
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        return view && view.document ? int(view.document.pageCount) : 0;
    }
#else
    return 0;
#endif
}

int PdfViewTab::currentPage() const
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        if (!view || !view.document || !view.currentPage)
            return 0;
        return int([view.document indexForPage:view.currentPage]);
    }
#else
    return 0;
#endif
}

void PdfViewTab::zoomIn()
{
    setZoomPercent(m_zoomPercent + 10);
}

void PdfViewTab::zoomOut()
{
    setZoomPercent(m_zoomPercent - 10);
}

void PdfViewTab::zoomReset()
{
    setZoomPercent(100);
}

void PdfViewTab::setZoomPercent(int percent)
{
    percent = qBound(50, percent, 400);
    if (percent == m_zoomPercent)
        return;
    m_zoomPercent = percent;
    applyZoom();
    emit zoomChanged(m_zoomPercent);
}

void PdfViewTab::goToPreviousPage()
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        if (view)
            [view goToPreviousPage:nil];
        syncPageSignal();
    }
#endif
}

void PdfViewTab::goToNextPage()
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        if (view)
            [view goToNextPage:nil];
        syncPageSignal();
    }
#endif
}

void PdfViewTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    ensureNativeView();
    layoutNativeView();
}

void PdfViewTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutNativeView();
}

void PdfViewTab::ensureNativeView()
{
#ifdef Q_OS_MACOS
    if (m_pdfView)
        return;
    @autoreleasepool {
        winId(); // force native window
        NSView *host = (__bridge NSView *)reinterpret_cast<void *>(winId());
        PDFView *view = [[PDFView alloc] initWithFrame:host.bounds];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.autoScales = NO;
        view.displayMode = kPDFDisplaySinglePageContinuous;
        view.displayDirection = kPDFDisplayDirectionVertical;
        view.backgroundColor = [NSColor colorWithCalibratedWhite:0.35 alpha:1.0];
        [host addSubview:view];
        m_pdfView = view; // retained by alloc/init; released in destructor
    }
#endif
}

void PdfViewTab::layoutNativeView()
{
#ifdef Q_OS_MACOS
    if (!m_pdfView)
        return;
    @autoreleasepool {
        NSView *host = (__bridge NSView *)reinterpret_cast<void *>(winId());
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        view.frame = host.bounds;
    }
#endif
}

void PdfViewTab::applyZoom()
{
#ifdef Q_OS_MACOS
    if (!m_pdfView)
        return;
    @autoreleasepool {
        PDFView *view = static_cast<PDFView *>(m_pdfView);
        view.autoScales = NO;
        const CGFloat factor = m_zoomPercent / 100.0;
        // Assign twice: some PDFKit versions ignore the first change while laying out.
        view.scaleFactor = factor;
        view.scaleFactor = factor;
    }
#endif
}

void PdfViewTab::syncPageSignal()
{
    emit pageChanged(currentPage(), pageCount());
}
