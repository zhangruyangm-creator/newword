#ifndef TEXTSTATS_H
#define TEXTSTATS_H

#include <QString>

/** Document / selection text metrics (Word/WPS-style for CJK + Latin). */
namespace TextStats {

struct Counts {
    int words = 0;            //!< 字数：每个汉字计 1，英文/数字按词
    int chars = 0;            //!< 字符（不计空白）
    int charsWithSpaces = 0;  //!< 字符（计空白）
    int punctuation = 0;      //!< 标点符号
    int cjkChars = 0;         //!< 汉字等 CJK 字符
    int spaces = 0;           //!< 空白字符（含换行）
};

[[nodiscard]] Counts analyze(QStringView text);

} // namespace TextStats

#endif // TEXTSTATS_H
