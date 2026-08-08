#ifndef FLOATINGTEXTBOX_H
#define FLOATINGTEXTBOX_H

#include <QString>
#include <QVector>

class QTextDocument;

/** Absolute-positioned text boxes (edit overlay + preview/PDF). Word-compatible DrawingML not used. */
struct FloatingTextBox {
    QString id;
    int pageIndex = 0; //!< 0-based paper page
    qreal xPt = 72;
    qreal yPt = 72; //!< relative to page content box
    qreal wPt = 180;
    qreal hPt = 90;
    QString html;
};

namespace FloatingTextBoxes {

inline constexpr char kResourceUrl[] = "newword://floating-textboxes";
inline constexpr char kHtmlMarkerPrefix[] = "<!--NEWWORD-FLOATBOXES:";
inline constexpr char kHtmlMarkerSuffix[] = "-->";
inline constexpr char kDocxPartPath[] = "customXml/newwordFloatingBoxes.xml";

[[nodiscard]] QVector<FloatingTextBox> load(const QTextDocument *document);
void save(QTextDocument *document, const QVector<FloatingTextBox> &boxes, bool markModified = true);

[[nodiscard]] FloatingTextBox makeDefault(int pageIndex = 0);
[[nodiscard]] QString toJson(const QVector<FloatingTextBox> &boxes);
[[nodiscard]] QVector<FloatingTextBox> fromJson(const QString &json);

//! Append / extract HTML comment payload used by HTML save and crash drafts.
[[nodiscard]] QString embedInHtml(const QString &html, const QVector<FloatingTextBox> &boxes);
[[nodiscard]] QVector<FloatingTextBox> extractFromHtml(const QString &html);
[[nodiscard]] QString stripMarkerFromHtml(const QString &html);

[[nodiscard]] QByteArray toXmlBytes(const QVector<FloatingTextBox> &boxes);
[[nodiscard]] QVector<FloatingTextBox> fromXmlBytes(const QByteArray &bytes);

} // namespace FloatingTextBoxes

#endif // FLOATINGTEXTBOX_H
