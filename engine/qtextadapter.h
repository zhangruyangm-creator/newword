#ifndef ENGINE_QTEXTADAPTER_H
#define ENGINE_QTEXTADAPTER_H

#include "documentmodel.h"

class QTextDocument;

namespace Engine {

/** Bidirectional bridge: QTextDocument ↔ DocumentModel. */
namespace QTextAdapter {

[[nodiscard]] DocumentModel fromDocument(const QTextDocument *document,
                                         const PageLayoutSettings &pageSetup = {});

//! Rebuild a QTextDocument from DocumentModel (DOCX closed-loop import path).
void toDocument(const DocumentModel &model, QTextDocument *document);

//! Caches the last DocumentModel and rebuilds only from a dirty document offset to the end.
//! Typical typing at the caret near the end stays O(changed suffix) on the GUI thread.
class SnapshotCache
{
public:
    void invalidate();

    [[nodiscard]] bool matches(int documentRevision, const PageLayoutSettings &setup) const;

    //! Record the earliest changed offset since the last successful ensure().
    void noteChange(int position);

    //! Return a model for `document` at its current revision (copy suitable for worker threads).
    //! Uses a suffix rebuild when a dirty hint is available and the prefix can be reused.
    [[nodiscard]] DocumentModel ensure(const QTextDocument *document,
                                       const PageLayoutSettings &pageSetup);

private:
    [[nodiscard]] bool pageSetupEquals(const PageLayoutSettings &a,
                                       const PageLayoutSettings &b) const;
    DocumentModel fullRebuild(const QTextDocument *document, const PageLayoutSettings &pageSetup);
    DocumentModel suffixRebuild(const QTextDocument *document,
                                const PageLayoutSettings &pageSetup,
                                int fromPos);

    DocumentModel m_model;
    PageLayoutSettings m_setup;
    int m_revision = -1;
    bool m_valid = false;
    bool m_hasDirtyHint = false;
    int m_dirtyPos = 0;
};

} // namespace QTextAdapter

} // namespace Engine

#endif // ENGINE_QTEXTADAPTER_H
