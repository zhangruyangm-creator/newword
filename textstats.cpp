#include "textstats.h"

namespace TextStats {
namespace {

bool isLineBreak(QChar ch)
{
    return ch == QChar::ParagraphSeparator
        || ch == QChar::LineSeparator
        || ch == QLatin1Char('\n')
        || ch == QLatin1Char('\r');
}

bool isCjkChar(QChar ch)
{
    switch (ch.script()) {
    case QChar::Script_Han:
    case QChar::Script_Hangul:
    case QChar::Script_Hiragana:
    case QChar::Script_Katakana:
    case QChar::Script_Bopomofo:
        return true;
    default:
        break;
    }
    // Compatibility ideographs / punctuation-adjacent CJK blocks without Script_Han.
    const uint u = ch.unicode();
    return (u >= 0x3400 && u <= 0x4DBF)
        || (u >= 0xF900 && u <= 0xFAFF);
}

bool isPunctuation(QChar ch)
{
    if (ch.isPunct())
        return true;
    // Full-width / CJK punctuation often classified as Other_Symbol / Other_Letter.
    switch (ch.unicode()) {
    case 0x3001: // 、
    case 0x3002: // 。
    case 0x3008: case 0x3009:
    case 0x300A: case 0x300B:
    case 0x300C: case 0x300D:
    case 0x300E: case 0x300F:
    case 0x3010: case 0x3011:
    case 0x3014: case 0x3015:
    case 0xFF01: // ！
    case 0xFF0C: // ，
    case 0xFF0E: // ．
    case 0xFF1A: // ：
    case 0xFF1B: // ；
    case 0xFF1F: // ？
    case 0x2014: // —
    case 0x2018: case 0x2019:
    case 0x201C: case 0x201D:
    case 0x2026: // …
        return true;
    default:
        return false;
    }
}

} // namespace

Counts analyze(QStringView text)
{
    Counts c;
    c.charsWithSpaces = int(text.size());
    bool inLatinWord = false;

    for (QChar ch : text) {
        if (ch.isSpace() || isLineBreak(ch)) {
            ++c.spaces;
            inLatinWord = false;
            continue;
        }

        ++c.chars;

        if (isPunctuation(ch)) {
            ++c.punctuation;
            inLatinWord = false;
            continue;
        }

        if (isCjkChar(ch)) {
            ++c.cjkChars;
            ++c.words;
            inLatinWord = false;
            continue;
        }

        if (ch.isLetterOrNumber()) {
            if (!inLatinWord) {
                ++c.words;
                inLatinWord = true;
            }
            continue;
        }

        inLatinWord = false;
    }

    return c;
}

} // namespace TextStats
