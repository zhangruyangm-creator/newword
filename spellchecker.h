#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include <QString>
#include <QStringList>

namespace SpellChecker {

bool isAvailable();
bool isMisspelled(const QString &word);
QStringList suggestions(const QString &word, int maxCount = 5);
void clearCache();

} // namespace SpellChecker

#endif // SPELLCHECKER_H
