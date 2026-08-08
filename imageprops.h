#ifndef IMAGEPROPS_H
#define IMAGEPROPS_H

#include <QTextFormat>
#include <QTextImageFormat>
#include <Qt>

/** Image display / wrap metadata stored on QTextImageFormat (UserProperty). */
namespace ImageProps {

enum class Wrap {
    Inline = 0,     //!< In-line with text (Qt default); PDF may still break onto own line mid-run
    Block = 1,      //!< Own paragraph; live ≈ engine
    FloatLeft = 2,  //!< Square wrap left (engine/PDF); live approximates as block
    FloatRight = 3  //!< Square wrap right (engine/PDF)
};

inline constexpr int kWrapProperty = int(QTextFormat::UserProperty) + 40;
inline constexpr int kAlignProperty = int(QTextFormat::UserProperty) + 41;

[[nodiscard]] Wrap wrapOf(const QTextImageFormat &fmt);
void setWrap(QTextImageFormat *fmt, Wrap wrap);

[[nodiscard]] Qt::Alignment alignOf(const QTextImageFormat &fmt);
void setAlign(QTextImageFormat *fmt, Qt::Alignment align);

/** Cap display size to page content width (px at 96dpi). */
void fitToMaxWidth(QTextImageFormat *fmt, const QImage &image, int maxWidthPx);

} // namespace ImageProps

#endif // IMAGEPROPS_H
