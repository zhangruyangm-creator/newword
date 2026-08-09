#include "spellchecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>

#include <hunspell/hunspell.hxx>

#include <cstdlib>
#include <vector>

namespace SpellChecker {
namespace {

QHash<QString, bool> &misspellCache()
{
    static QHash<QString, bool> cache;
    return cache;
}

constexpr int kMaxCacheEntries = 4096;

QMutex &mutex()
{
    static QMutex m;
    return m;
}

Hunspell *&spellInstance()
{
    static Hunspell *instance = nullptr;
    return instance;
}

//! Candidate dictionary base names (Hunspell looks up <name>.aff + <name>.dic).
QStringList candidateNames()
{
    return {QStringLiteral("en_US"), QStringLiteral("en"), QStringLiteral("index")};
}

//! Ordered search locations for the dictionary directory.
QStringList dictionaryCandidates()
{
    QStringList dirs;

    if (const char *env = std::getenv("HUNSPELL_DICT_PATH"))
        dirs << QString::fromUtf8(env);

    const QString appDir = QCoreApplication::applicationDirPath();
    dirs << appDir + QStringLiteral("/../Resources/dictionaries");
    dirs << appDir + QStringLiteral("/dictionaries");

#ifdef NEWWORD_DICT_DIR
    dirs << QStringLiteral(NEWWORD_DICT_DIR);
#endif

    dirs << QStringLiteral("/opt/homebrew/share/hunspell")
         << QStringLiteral("/usr/local/share/hunspell")
         << QStringLiteral("/usr/share/hunspell");
    return dirs;
}

bool findDictionary(QString *affPath, QString *dicPath)
{
    const QStringList dirs = dictionaryCandidates();
    const QStringList names = candidateNames();
    for (const QString &dir : dirs) {
        const QDir d(dir);
        if (!d.exists())
            continue;
        for (const QString &name : names) {
            const QString aff = d.filePath(name + QStringLiteral(".aff"));
            const QString dic = d.filePath(name + QStringLiteral(".dic"));
            if (QFileInfo::exists(aff) && QFileInfo::exists(dic)) {
                *affPath = aff;
                *dicPath = dic;
                return true;
            }
        }
    }
    return false;
}

bool ensureLoaded()
{
    QMutexLocker locker(&mutex());
    if (spellInstance())
        return true;

    QString affPath, dicPath;
    if (!findDictionary(&affPath, &dicPath))
        return false;

    auto *hunspell =
        new Hunspell(affPath.toUtf8().constData(), dicPath.toUtf8().constData());
    // Probe with a common word: a failed/empty dictionary would mark it
    // misspelled, so treat that as "not available".
    if (!hunspell->spell(std::string("the"))) {
        delete hunspell;
        return false;
    }
    spellInstance() = hunspell;
    return true;
}

bool checkMisspelledUncached(const QString &word)
{
    QMutexLocker locker(&mutex());
    Hunspell *hunspell = spellInstance();
    if (!hunspell)
        return false;
    const QByteArray utf8 = word.toUtf8();
    return !hunspell->spell(std::string(utf8.constData(), utf8.size()));
}

} // namespace

bool isAvailable()
{
    return ensureLoaded();
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

    if (!ensureLoaded())
        return false;

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

    if (!ensureLoaded())
        return result;

    QMutexLocker locker(&mutex());
    Hunspell *hunspell = spellInstance();
    if (!hunspell)
        return result;

    const QByteArray utf8 = word.toUtf8();
    const std::vector<std::string> guesses =
        hunspell->suggest(std::string(utf8.constData(), utf8.size()));
    for (const std::string &guess : guesses) {
        result << QString::fromUtf8(guess.c_str());
        if (result.size() >= maxCount)
            break;
    }
    return result;
}

} // namespace SpellChecker
