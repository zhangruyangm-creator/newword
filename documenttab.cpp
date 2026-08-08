#include "documenttab.h"
#include "appstyle.h"
#include "documentrecovery.h" // EditorDefaults
#include "formularenderer.h"
#include "outlineviewwidget.h"
#include "pagegeometry.h"
#include "rulerwidget.h"
#include "spellchecker.h"
#include "spellhighlighter.h"

#include <QEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

DocumentTab::DocumentTab(QWidget *parent)
    : QWidget(parent)
    , m_recoveryId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_stack = new QStackedWidget(this);

    // ---- Page host (real paper view) ----
    m_pageHost = new QWidget(m_stack);
    m_ruler = new RulerWidget(m_pageHost);

    auto *doc = new QTextDocument(m_pageHost);
    doc->setUndoRedoEnabled(true);
    doc->setDefaultFont(EditorDefaults::documentFont());
    m_editor = new PagedEditorWidget(doc, m_pageLayout, m_headerFooter, m_pageHost);
    m_editor->setAcceptRichText(true);
    m_editor->setTabStopDistance(40);
    m_editor->setContextMenuPolicy(Qt::CustomContextMenu);
    m_editor->setStyleSheet(QStringLiteral("background: transparent;"));

    m_highlighter = new SpellHighlighter(doc);

    auto *pageLayout = new QVBoxLayout(m_pageHost);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(m_ruler);
    pageLayout->addWidget(m_editor, 1);

    // ---- Continuous host (draft / web / reading) ----
    m_continuousHost = new QWidget(m_stack);
    m_continuousHost->setObjectName(QStringLiteral("continuousHost"));
    m_continuousLayout = new QVBoxLayout(m_continuousHost);
    m_continuousLayout->setContentsMargins(0, 0, 0, 0);
    m_continuousLayout->setSpacing(0);

    // ---- Outline view ----
    m_outlineView = new OutlineViewWidget(m_stack);

    m_stack->addWidget(m_pageHost);
    m_stack->addWidget(m_continuousHost);
    m_stack->addWidget(m_outlineView);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    connect(doc, &QTextDocument::contentsChanged, this, [this]() {
        emit contentsChanged();
        emit modificationChanged(m_editor->document()->isModified());
    });
    connect(m_editor, &PagedEditorWidget::cursorPositionChanged, this,
            &DocumentTab::cursorMoved);
    connect(m_editor, &PagedEditorWidget::pageInfoChanged, this,
            &DocumentTab::pageInfoChanged);
    connect(m_editor, &PagedEditorWidget::imageDropped, this,
            &DocumentTab::insertImageFile);
    connect(m_editor, &PagedEditorWidget::scrolled, this, [this]() { syncRuler(); });
    connect(m_editor, &PagedEditorWidget::pageGeometryChanged, this, [this]() { syncRuler(); });
    connect(m_outlineView, &OutlineViewWidget::documentEdited, this, [this]() {
        m_editor->document()->setModified(true);
        emit contentsChanged();
        emit modificationChanged(true);
    });
    connect(m_editor, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                if (m_viewMode == DocumentViewMode::Reading)
                    return;
                QMenu *menu = m_editor->createStandardContextMenu();
                if (m_spellCheckEnabled && SpellChecker::isAvailable()) {
                    QTextCursor cursor = m_editor->cursorForPosition(pos);
                    cursor.select(QTextCursor::WordUnderCursor);
                    const QString word = cursor.selectedText();
                    if (SpellChecker::isMisspelled(word)) {
                        menu->addSeparator();
                        const QStringList guesses = SpellChecker::suggestions(word);
                        if (guesses.isEmpty()) {
                            auto *none = menu->addAction(tr("无拼写建议"));
                            none->setEnabled(false);
                        } else {
                            for (const QString &guess : guesses) {
                                QAction *fix = menu->addAction(guess);
                                connect(fix, &QAction::triggered, this,
                                        [cursor, guess]() mutable {
                                            QTextCursor c = cursor;
                                            c.insertText(guess);
                                        });
                            }
                        }
                    }
                }
                if (m_editor->textCursor().currentTable() && !m_tableActions.isEmpty()) {
                    menu->addSeparator();
                    auto *tableMenu = menu->addMenu(tr("表格"));
                    for (QAction *action : m_tableActions) {
                        if (action)
                            tableMenu->addAction(action);
                    }
                }

                QTextCursor formulaCursor = m_editor->cursorForPosition(pos);
                QString formulaLatex;
                int formulaPos = -1;
                auto tryFormulaAt = [&](int docPos) {
                    if (docPos < 0)
                        return false;
                    QTextBlock block = m_editor->document()->findBlock(docPos);
                    for (auto it = block.begin(); !(it.atEnd()); ++it) {
                        const QTextFragment frag = it.fragment();
                        if (!frag.isValid() || !frag.charFormat().isImageFormat())
                            continue;
                        const int start = frag.position();
                        const int end = start + frag.length();
                        if (docPos < start || docPos >= end)
                            continue;
                        const QString name = frag.charFormat().toImageFormat().name();
                        if (!FormulaRenderer::isFormulaResource(name))
                            continue;
                        formulaLatex = FormulaRenderer::latexFromResourceName(name);
                        formulaPos = start;
                        return true;
                    }
                    return false;
                };
                if (!tryFormulaAt(formulaCursor.position()) && formulaCursor.position() > 0)
                    tryFormulaAt(formulaCursor.position() - 1);
                if (!formulaLatex.isEmpty()) {
                    menu->addSeparator();
                    QAction *editFormula = menu->addAction(tr("编辑公式…"));
                    connect(editFormula, &QAction::triggered, this,
                            [this, formulaLatex, formulaPos]() {
                                emit editFormulaRequested(formulaLatex, formulaPos);
                            });
                }

                menu->exec(m_editor->mapToGlobal(pos));
                menu->deleteLater();
            });

    setViewMode(DocumentViewMode::Page);
}

DocumentTab::~DocumentTab() = default;

void DocumentTab::setTableContextActions(const QList<QAction *> &actions)
{
    m_tableActions = actions;
}

void DocumentTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    syncRuler();
}

void DocumentTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncRuler();
}

void DocumentTab::setFilePath(const QString &path)
{
    m_filePath = path;
}

QString DocumentTab::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("未命名");
    return QFileInfo(m_filePath).fileName();
}

bool DocumentTab::isModified() const
{
    return m_editor->document()->isModified();
}

void DocumentTab::setHeaderFooter(const HeaderFooterSettings &settings)
{
    m_headerFooter = settings;
    if (m_editor)
        m_editor->setHeaderFooter(settings);
}

void DocumentTab::setHeaderFooter(const QString &header, const QString &footer,
                                  bool showPageNumber)
{
    m_headerFooter.header = header;
    m_headerFooter.footer = footer;
    m_headerFooter.showPageNumber = showPageNumber;
    if (m_editor)
        m_editor->setHeaderFooter(m_headerFooter);
}

void DocumentTab::setPageLayout(const PageLayoutSettings &layout)
{
    m_pageLayout = layout;
    if (m_editor)
        m_editor->setPageLayout(layout);
    syncRuler();
    emit pageLayoutChanged();
}

void DocumentTab::setSpellCheckEnabled(bool enabled)
{
    m_spellCheckEnabled = enabled;
    if (m_highlighter) {
        m_highlighter->setEnabled(enabled && m_viewMode != DocumentViewMode::Reading
                                  && m_viewMode != DocumentViewMode::Outline);
    }
}

void DocumentTab::setZoomPercent(int percent)
{
    percent = qBound(50, percent, 200);
    if (percent == m_zoomPercent)
        return;
    m_zoomPercent = percent;
    if (m_editor)
        m_editor->setZoomPercent(percent);
    syncRuler();
    emit zoomChanged(m_zoomPercent);
}

void DocumentTab::beginBulkDocumentUpdate()
{
    if (m_highlighter)
        m_highlighter->setSuspended(true);
}

void DocumentTab::endBulkDocumentUpdate()
{
    if (m_highlighter)
        m_highlighter->setSuspended(false); // starts idle chunked rehighlight
}

void DocumentTab::placeEditorInPageFrame()
{
    if (m_editor->parentWidget() == m_pageHost)
        return;
    m_continuousLayout->removeWidget(m_editor);
    m_editor->setParent(m_pageHost);
    if (auto *lay = qobject_cast<QVBoxLayout *>(m_pageHost->layout()))
        lay->addWidget(m_editor, 1);
    m_editor->setPageMode(true);
    m_editor->show();
}

void DocumentTab::placeEditorContinuous()
{
    if (m_editor->parentWidget() == m_continuousHost)
        return;
    if (auto *lay = qobject_cast<QVBoxLayout *>(m_pageHost->layout()))
        lay->removeWidget(m_editor);
    m_editor->setParent(m_continuousHost);
    m_continuousLayout->addWidget(m_editor);
    m_editor->setPageMode(false);
    m_editor->show();
}

void DocumentTab::setViewMode(DocumentViewMode mode)
{
    if (m_viewMode == mode)
        return;
    m_viewMode = mode;

    if (mode == DocumentViewMode::Outline) {
        m_outlineView->setDocument(m_editor->document());
        m_stack->setCurrentWidget(m_outlineView);
    } else if (mode == DocumentViewMode::Page) {
        placeEditorInPageFrame();
        m_stack->setCurrentWidget(m_pageHost);
        m_editor->setReadOnly(false);
        m_editor->setFocus(Qt::OtherFocusReason);
    } else {
        placeEditorContinuous();
        m_stack->setCurrentWidget(m_continuousHost);
        m_editor->setReadOnly(mode == DocumentViewMode::Reading);
        if (mode != DocumentViewMode::Reading)
            m_editor->setFocus(Qt::OtherFocusReason);
    }

    if (m_highlighter) {
        m_highlighter->setEnabled(m_spellCheckEnabled
                                  && mode != DocumentViewMode::Reading
                                  && mode != DocumentViewMode::Outline);
    }

    emit viewModeChanged(mode);
    updateRulerVisibility();
    syncRuler();
}

void DocumentTab::setRulerVisible(bool visible)
{
    m_rulerVisible = visible;
    updateRulerVisibility();
}

void DocumentTab::setGridLinesVisible(bool visible)
{
    m_showGridLines = visible;
    // Grid overlay is not implemented in the self-drawn editor yet.
}

void DocumentTab::updateRulerVisibility()
{
    if (!m_ruler)
        return;
    m_ruler->setVisible(m_rulerVisible && m_viewMode == DocumentViewMode::Page);
}

void DocumentTab::syncRuler()
{
    if (!m_ruler || !m_editor || m_viewMode != DocumentViewMode::Page)
        return;
    const PageGeometry geo = PageGeometry::from(m_pageLayout, m_editor->zoomPercent());
    m_ruler->setPageWidthPx(geo.pageWidthPx);
    m_ruler->setMarginsMm(m_pageLayout.marginsMm);
    m_ruler->setZoomFactor(geo.zoomFactor);
    m_ruler->setOffsetPx(m_editor->pageOffsetX());
}

void DocumentTab::insertFloatingTextBox()
{
    // Floating text boxes are not rendered by the self-drawn editor yet;
    // their data still round-trips through the document for file compatibility.
}

void DocumentTab::reloadFloatingTextBoxes()
{
    // No-op while the floating box layer is disabled.
}

void DocumentTab::refreshTheme()
{
    if (m_ruler)
        m_ruler->refreshTheme();
    if (m_outlineView)
        m_outlineView->refreshTheme();
    if (m_editor)
        m_editor->update();
}
