#ifndef DOCXCOMMON_H
#define DOCXCOMMON_H

#include "styleutils.h"

#include <QString>

namespace DocxCommon {

QString xmlEscape(const QString &text);
QString styleIdToDocx(StyleUtils::StyleId id);
StyleUtils::StyleId docxStyleToId(const QString &name);

} // namespace DocxCommon

#endif // DOCXCOMMON_H
