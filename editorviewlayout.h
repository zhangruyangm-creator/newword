#ifndef EDITORVIEWLAYOUT_H
#define EDITORVIEWLAYOUT_H

#include "documentviewmode.h"
#include "pagelayout.h"
#include "pagegeometry.h"
#include "pagelist.h"
#include "qtextadapter.h"

#include <QVector>

class QFrame;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;
class QWidget;
class RulerWidget;

//! Single entry point for editor chrome + page geometry.
//! All view/zoom layout mutations for the live editor should go through here.
class EditorViewLayout
{
public:
    struct Hosts {
        QTextEdit *editor = nullptr;
        QWidget *continuousHost = nullptr;
        QVBoxLayout *continuousLayout = nullptr;
        QScrollArea *pageScroll = nullptr;
        RulerWidget *ruler = nullptr;
        QFrame *pageFrame = nullptr;
        QWidget *headerChrome = nullptr;
        QWidget *footerChrome = nullptr;
    };

    struct PageApplyResult {
        int pageCount = 1;       //!< Prefer LayoutEngine; Fast may use geometric interim
        int pageWidthPx = 0;
        int editorHeightPx = 0;
        int onePageBodyPx = 0;   //!< Content-box height (fallback geometric seams)
        QVector<int> pageBreakDocPositions; //!< Engine page starts (doc offsets)
        bool usedEngine = false; //!< true when pageCount/seams came from LayoutEngine this call
    };

    enum class GeometryMode {
        //! Cheap: resize strip from QTextDocument height; reuse last engine seams if any.
        Fast,
        //! Full Adapter + LayoutEngine (aligned with preview/PDF).
        Precise
    };

    explicit EditorViewLayout(Hosts hosts);

    void setPageLayout(const PageLayoutSettings &layout);
    PageLayoutSettings pageLayout() const { return m_pageLayout; }

    void invalidateCache();

    //! Earliest document offset changed since last adapter snapshot (for suffix rebuild).
    void noteDocumentChange(int position);

    //! GUI-thread snapshot for LayoutEngine (incremental when possible).
    [[nodiscard]] Engine::DocumentModel ensureDocumentModel(QTextDocument *document);

    //! Continuous views: QTextEdit font zoom steps relative to 100%.
    void setFontZoomSteps(int targetSteps);
    int fontZoomSteps() const { return m_fontZoomSteps; }

    //! Apply scrollbars / stylesheet / margins for non-page modes (and page chrome shell).
    void applyChrome(DocumentViewMode mode, int hostWidthPx);

    //! Page view: paper size, document margin, continuous pageSize, editor/frame heights.
    PageApplyResult applyPageGeometry(int zoomPercent,
                                      GeometryMode mode = GeometryMode::Precise,
                                      bool force = false);

    [[nodiscard]] bool engineCacheMatches(int documentRevision,
                                          const PageLayoutSettings &setup) const;
    //! Apply a LayoutEngine result computed off-thread (must match current doc revision).
    void storeEngineResult(int documentRevision,
                           const PageLayoutSettings &setup,
                           const Engine::LayoutResult &layout);

    void updateRuler(int zoomPercent);
    void updatePageBorderChrome();

private:
    [[nodiscard]] bool pageSetupEquals(const PageLayoutSettings &a, const PageLayoutSettings &b) const;
    void applyFrameHeights(const PageGeometry &geo, int headerH, int footerH, PageApplyResult *result);

    Hosts m_hosts;
    PageLayoutSettings m_pageLayout;
    int m_fontZoomSteps = 0;

    int m_cachedPageWidth = -1;
    int m_cachedDocHeight = -1;
    int m_cachedMargin = -1;
    int m_cachedZoomPercent = -1;
    int m_cachedPageCountVisual = -1;
    //! Strip height from the last engine-backed layout. Fast geometry never
    //! shrinks below it, so pagination correction adds space instead of moving
    //! content and clamping the scrollbar while Precise is stale.
    int m_engineStripFloorPx = 0;

    //! Engine result cache (document offsets — zoom-independent).
    int m_cachedEngineRevision = -1;
    PageLayoutSettings m_cachedEnginePageSetup;
    int m_cachedEnginePageCount = 1;
    QVector<int> m_cachedEngineBreaks;
    bool m_hasEngineCache = false;

    Engine::QTextAdapter::SnapshotCache m_adapterCache;
};

#endif // EDITORVIEWLAYOUT_H
