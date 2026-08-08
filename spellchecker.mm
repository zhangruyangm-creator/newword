#include "spellchecker.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#endif

namespace SpellChecker {
namespace {

QHash<QString, bool> &misspellCache()
{
    static QHash<QString, bool> cache;
    return cache;
}

constexpr int kMaxCacheEntries = 4096;

bool checkMisspelledUncached(const QString &word)
{
#ifdef Q_OS_MACOS
    @autoreleasepool {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        NSString *nsWord = word.toNSString();
        const NSRange range = [checker checkSpellingOfString:nsWord
                                                  startingAt:0
                                                    language:nil
                                                        wrap:NO
                                      inSpellDocumentWithTag:0
                                                   wordCount:nullptr];
        return range.location != NSNotFound;
    }
#else
    Q_UNUSED(word)
    return false;
#endif
}

} // namespace

bool isAvailable()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

bool isMisspelled(const QString &word)
{
    if (word.size() < 2)
        return false;

    static const QRegularExpression latinWord(QStringLiteral("^[A-Za-z][A-Za-z'\\-]*$"));
    if (!latinWord.match(word).hasMatch())
        return false;

    auto &cache = misspellCache();
    const auto it = cache.constFind(word);
    if (it != cache.cend())
        return it.value();

    const bool misspelled = checkMisspelledUncached(word);
    if (cache.size() >= kMaxCacheEntries)
        cache.clear();
    cache.insert(word, misspelled);
    return misspelled;
}

void clearCache()
{
    misspellCache().clear();
}

QStringList suggestions(const QString &word, int maxCount)
{
    QStringList result;
#ifdef Q_OS_MACOS
    @autoreleasepool {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        NSString *nsWord = word.toNSString();
        NSArray<NSString *> *guesses =
            [checker guessesForWordRange:NSMakeRange(0, nsWord.length)
                                inString:nsWord
                                language:nil
                  inSpellDocumentWithTag:0];
        int count = 0;
        for (NSString *guess in guesses) {
            result << QString::fromNSString(guess);
            if (++count >= maxCount)
                break;
        }
    }
#else
    Q_UNUSED(word)
    Q_UNUSED(maxCount)
#endif
    return result;
}

} // namespace SpellChecker
