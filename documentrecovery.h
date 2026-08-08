#ifndef DOCUMENTRECOVERY_H
#define DOCUMENTRECOVERY_H

#include "headerfootersettings.h"
#include "pagelayout.h"

#include <QDateTime>
#include <QFont>
#include <QString>
#include <QVector>

class QTextDocument;

/** Local draft snapshots for crash / abnormal-exit recovery. */
namespace DocumentRecovery {

struct Draft {
    QString id;
    QString sourcePath; //!< empty = untitled
    QString displayName;
    QDateTime savedAt;
    HeaderFooterSettings headerFooter;
    PageLayoutSettings pageLayout;
    QString htmlFilePath;
};

[[nodiscard]] QString draftsDirectory();
[[nodiscard]] QVector<Draft> listDrafts();
[[nodiscard]] bool writeDraft(const QString &id,
                              const QString &sourcePath,
                              const QString &displayName,
                              QTextDocument *document,
                              const HeaderFooterSettings &headerFooter,
                              const PageLayoutSettings &pageLayout,
                              QString *errorMessage = nullptr);
[[nodiscard]] bool loadDraftHtml(const Draft &draft, QString *html, QString *errorMessage = nullptr);
bool removeDraft(const QString &id);
void removeAllDrafts();

} // namespace DocumentRecovery

/** Persisted prefs for new blank documents. */
namespace EditorDefaults {

[[nodiscard]] QFont documentFont();
[[nodiscard]] PageLayoutSettings pageLayout();
[[nodiscard]] HeaderFooterSettings headerFooter();
void loadFromSettings();
void saveToSettings(const QFont &font, const PageLayoutSettings &layout,
                    const HeaderFooterSettings &headerFooter);

} // namespace EditorDefaults

#endif // DOCUMENTRECOVERY_H
