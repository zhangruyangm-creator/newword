#include "editorviewlayout.h"
#include "appstyle.h"
#include "layoutengine.h"
#include "rulerwidget.h"

#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <utility>

namespace {

bool nearlyEqual(qreal a, qreal b)
{
    return std::abs(a - b) < 0.05;
}

} // namespace

EditorViewLayout::EditorViewLayout(Hosts hosts)
    : m_hosts(std::move(hosts))
{
}

void EditorViewLayout::setPageLayout(const PageLayoutSettings &layout)
{
    m_pageLayout = layout;
    invalidateCache();
}

void EditorViewLayout::invalidateCache()
{
    m_cachedPageWidth = -1;
    m_cachedDocHeight = -1;
    m_cachedMargin = -1;
    m_cachedZoomPercent = -1;
    m_cachedPageCountVisual = -1;
    m_cachedEngineRevision = -1;
    m_hasEngineCache = false;
    m_cachedEngineBreaks.clear();
    m_cachedEnginePageCount = 1;
    m_adapterCache.invalidate();
}

void EditorViewLayout::noteDocumentChange(int position)
{
    m_adapterCache.noteChange(position);
}

Engine::DocumentModel EditorViewLayout::ensureDocumentModel(QTextDocument *document)
{
    return m_adapterCache.ensure(document, m_pageLayout);
}

bool EditorViewLayout::engineCacheMatches(int documentRevision,
                                          const PageLayoutSettings &setup) const
{
    return m_hasEngineCache
        && documentRevision == m_cachedEngineRevision
        && pageSetupEquals(m_pageLayout, setup)
        && pageSetupEquals(m_pageLayout, m_cachedEnginePageSetup);
}

void EditorViewLayout::storeEngineResult(int documentRevision,
                                         const PageLayoutSettings &setup,
                                         const Engine::LayoutResult &layout)
{
    m_cachedEnginePageCount = layout.pageCount();
    m_cachedEngineBreaks = layout.pageBreakDocPositions();
    m_cachedEngineRevision = documentRevision;
    m_cachedEnginePageSetup = setup;
    m_hasEngineCache = true;
}

bool EditorViewLayout::pageSetupEquals(const PageLayoutSettings &a, const PageLayoutSettings &b) const
{
    return a.paper == b.paper
        && a.orientation == b.orientation
        && nearlyEqual(a.customWidthMm, b.customWidthMm)
        && nearlyEqual(a.customHeightMm, b.customHeightMm)
        && nearlyEqual(a.marginsMm.left(), b.marginsMm.left())
        && nearlyEqual(a.marginsMm.top(), b.marginsMm.top())
        && nearlyEqual(a.marginsMm.right(), b.marginsMm.right())
        && nearlyEqual(a.marginsMm.bottom(), b.marginsMm.bottom())
        && nearlyEqual(a.headerDistanceMm, b.headerDistanceMm)
        && nearlyEqual(a.footerDistanceMm, b.footerDistanceMm)
        && a.columnCount == b.columnCount;
}

void EditorViewLayout::setFontZoomSteps(int targetSteps)
{
    if (!m_hosts.editor)
        return;
    const int delta = targetSteps - m_fontZoomSteps;
    if (delta > 0)
        m_hosts.editor->zoomIn(delta);
    else if (delta < 0)
        m_hosts.editor->zoomOut(-delta);
    m_fontZoomSteps = targetSteps;
}

void EditorViewLayout::applyChrome(DocumentViewMode mode, int hostWidthPx)
{
    if (!m_hosts.editor)
        return;

    switch (mode) {
    case DocumentViewMode::Page:
        // Ruler visibility is owned by DocumentTab (user preference).
        if (m_hosts.pageScroll)
            m_hosts.pageScroll->setStyleSheet(AppStyle::pageScrollStyleSheet());
        m_hosts.editor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: transparent; border: none; padding: 0px; }"));
        m_hosts.editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_hosts.editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        break;

    case DocumentViewMode::Draft:
        m_hosts.editor->setReadOnly(false);
        m_hosts.editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_hosts.editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_hosts.editor->setMinimumSize(0, 0);
        m_hosts.editor->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_hosts.editor->document()->setDocumentMargin(24);
        m_hosts.editor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: #ffffff; border: none; padding: 8px 16px; }"));
        if (m_hosts.continuousHost)
            m_hosts.continuousHost->setStyleSheet(
                AppStyle::continuousHostStyleSheet(QStringLiteral("#ffffff")));
        if (m_hosts.continuousLayout)
            m_hosts.continuousLayout->setAlignment(m_hosts.editor, Qt::Alignment());
        break;

    case DocumentViewMode::Web:
        m_hosts.editor->setReadOnly(false);
        m_hosts.editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_hosts.editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_hosts.editor->setMinimumSize(0, 0);
        m_hosts.editor->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_hosts.editor->document()->setDocumentMargin(20);
        m_hosts.editor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: #ffffff; border: none; padding: 12px 24px; }"));
        if (m_hosts.continuousHost)
            m_hosts.continuousHost->setStyleSheet(
                AppStyle::continuousHostStyleSheet(QStringLiteral("#ffffff")));
        if (m_hosts.continuousLayout)
            m_hosts.continuousLayout->setAlignment(m_hosts.editor, Qt::Alignment());
        break;

    case DocumentViewMode::Reading: {
        m_hosts.editor->setReadOnly(true);
        m_hosts.editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_hosts.editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const int maxW = qMin(720, qMax(480, hostWidthPx - 80));
        m_hosts.editor->setFixedWidth(maxW);
        m_hosts.editor->setMinimumHeight(0);
        m_hosts.editor->setMaximumHeight(QWIDGETSIZE_MAX);
        m_hosts.editor->document()->setDocumentMargin(28);
        m_hosts.editor->document()->setPageSize(QSizeF(maxW, -1));
        m_hosts.editor->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: #faf8f5;"
            "  border: none;"
            "  padding: 28px 36px;"
            "  font-size: 16px;"
            "}"));
        if (m_hosts.continuousHost)
            m_hosts.continuousHost->setStyleSheet(
                AppStyle::continuousHostStyleSheet(QStringLiteral("#e6e9ee")));
        if (m_hosts.continuousLayout)
            m_hosts.continuousLayout->setAlignment(m_hosts.editor, Qt::AlignHCenter | Qt::AlignTop);
        break;
    }

    case DocumentViewMode::Outline:
        break;
    }
}

void EditorViewLayout::applyFrameHeights(const PageGeometry &geo, int headerH, int footerH,
                                         PageApplyResult *result)
{
    if (!result || !m_hosts.editor || !m_hosts.pageFrame)
        return;
    if (result->editorHeightPx != m_cachedDocHeight || geo.pageWidthPx != m_hosts.pageFrame->width()
        || result->pageCount != m_cachedPageCountVisual) {
        m_hosts.editor->setFixedHeight(result->editorHeightPx);
        m_hosts.pageFrame->setFixedSize(geo.pageWidthPx,
                                        headerH + result->editorHeightPx + footerH);
        m_cachedDocHeight = result->editorHeightPx;
        m_cachedPageCountVisual = result->pageCount;
        updatePageBorderChrome();
    }
}

EditorViewLayout::PageApplyResult EditorViewLayout::applyPageGeometry(int zoomPercent,
                                                                      GeometryMode mode,
                                                                      bool force)
{
    PageApplyResult result;
    if (!m_hosts.editor || !m_hosts.pageFrame)
        return result;

    QTextDocument *doc = m_hosts.editor->document();
    if (!doc)
        return result;

    if (force)
        invalidateCache();

    const PageGeometry geo = PageGeometry::from(m_pageLayout, zoomPercent);
    result.pageWidthPx = geo.pageWidthPx;

    const bool sizeChanged = (geo.pageWidthPx != m_cachedPageWidth || geo.marginPx != m_cachedMargin
                              || zoomPercent != m_cachedZoomPercent);
    if (sizeChanged) {
        m_hosts.editor->setFixedWidth(geo.pageWidthPx);
        doc->setDocumentMargin(geo.marginPx);
        doc->setPageSize(QSizeF(geo.pageWidthPx, -1));
        m_cachedPageWidth = geo.pageWidthPx;
        m_cachedMargin = geo.marginPx;
        m_cachedZoomPercent = zoomPercent;
        m_cachedDocHeight = -1;
    }

    const int headerH = geo.chromeLabelHeightPx(m_hosts.headerChrome && m_hosts.headerChrome->isVisible());
    const int footerH = geo.chromeLabelHeightPx(m_hosts.footerChrome && m_hosts.footerChrome->isVisible());
    if (m_hosts.headerChrome && m_hosts.headerChrome->isVisible())
        m_hosts.headerChrome->setFixedHeight(headerH);
    if (m_hosts.footerChrome && m_hosts.footerChrome->isVisible())
        m_hosts.footerChrome->setFixedHeight(footerH);

    result.onePageBodyPx = PageGeometry::contentBodyHeightPx(m_pageLayout, zoomPercent);
    const int docHeight = qMax(1, qRound(doc->size().height()) + 24);
    const int revision = doc->revision();

    const bool engineCacheHit = m_hasEngineCache
        && !force
        && revision == m_cachedEngineRevision
        && pageSetupEquals(m_pageLayout, m_cachedEnginePageSetup);

    if (mode == GeometryMode::Precise && !engineCacheHit) {
        const Engine::DocumentModel model = m_adapterCache.ensure(doc, m_pageLayout);
        const Engine::LayoutResult engineLayout = Engine::LayoutEngine::layout(model, m_pageLayout);
        m_cachedEnginePageCount = engineLayout.pageCount();
        m_cachedEngineBreaks = engineLayout.pageBreakDocPositions();
        m_cachedEngineRevision = revision;
        m_cachedEnginePageSetup = m_pageLayout;
        m_hasEngineCache = true;
        result.usedEngine = true;
        result.pageCount = m_cachedEnginePageCount;
        result.pageBreakDocPositions = m_cachedEngineBreaks;
    } else if (engineCacheHit) {
        // Only reuse breaks when revision/setup still match — never after edits.
        result.usedEngine = true;
        result.pageCount = m_cachedEnginePageCount;
        result.pageBreakDocPositions = m_cachedEngineBreaks;
    } else {
        // Fast path or stale cache: geometric interim page count, no seam offsets.
        const int geoPages = qMax(1, (docHeight + result.onePageBodyPx - 1) / result.onePageBodyPx);
        result.pageCount = geoPages;
        result.pageBreakDocPositions.clear();
        result.usedEngine = false;
    }

    result.editorHeightPx = qMax(result.onePageBodyPx, docHeight);
    if (result.usedEngine) {
        // Full fixed-height sheets (gaps are visual overlays, not extra layout
        // space). Only the engine count is authoritative for this floor.
        result.editorHeightPx = qMax(result.editorHeightPx,
                                     result.pageCount * result.onePageBodyPx);
        m_engineStripFloorPx = result.editorHeightPx;
    } else {
        // Geometric interim: size the strip by the real content height and never
        // collapse below the last engine-backed height while Precise catches up.
        result.editorHeightPx = qMax(result.editorHeightPx, m_engineStripFloorPx);
    }

    applyFrameHeights(geo, headerH, footerH, &result);
    updateRuler(zoomPercent);
    return result;
}

void EditorViewLayout::updateRuler(int zoomPercent)
{
    if (!m_hosts.ruler || !m_hosts.pageFrame || !m_hosts.pageScroll)
        return;
    const int pageWidth = m_hosts.pageFrame->width();
    const int viewportWidth = m_hosts.pageScroll->viewport()->width();
    const int contentOffset = qMax(0, (viewportWidth - pageWidth) / 2)
                              - m_hosts.pageScroll->horizontalScrollBar()->value();
    m_hosts.ruler->setPageWidthPx(pageWidth);
    m_hosts.ruler->setMarginsMm(m_pageLayout.marginsMm);
    m_hosts.ruler->setZoomFactor(PageGeometry::zoomFactorFor(zoomPercent));
    m_hosts.ruler->setOffsetPx(contentOffset);
}

void EditorViewLayout::updatePageBorderChrome()
{
    if (!m_hosts.pageFrame)
        return;
    if (m_pageLayout.showPageBorder) {
        const int w = qMax(1, qRound(m_pageLayout.pageBorderWidthPt));
        m_hosts.pageFrame->setStyleSheet(
            AppStyle::pageFrameStyleSheet(true, w, m_pageLayout.pageBorderColor.name()));
    } else {
        m_hosts.pageFrame->setStyleSheet(AppStyle::pageFrameStyleSheet(false, 1, QString()));
    }
}
