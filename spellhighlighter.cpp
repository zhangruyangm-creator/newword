#include "spellhighlighter.h"
#include "spellchecker.h"

#include <QRegularExpression>
#include <QTextDocument>

SpellHighlighter::SpellHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_errorFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_errorFormat.setUnderlineColor(QColor(220, 50, 50));

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(0);
    connect(m_idleTimer, &QTimer::timeout, this, &SpellHighlighter::rehighlightNextChunk);
}

void SpellHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (m_suspended)
        return;
    if (m_enabled)
        rehighlightIdle();
    else
        rehighlight(); // clear underlines in one pass
}

void SpellHighlighter::setSuspended(bool suspended)
{
    if (m_suspended == suspended)
        return;
    m_suspended = suspended;
    if (m_idleTimer)
        m_idleTimer->stop();
    if (!m_suspended && m_enabled)
        rehighlightIdle();
}

void SpellHighlighter::rehighlightIdle()
{
    if (!document() || m_suspended || !m_enabled) {
        if (m_idleTimer)
            m_idleTimer->stop();
        return;
    }
    m_idleBlock = document()->begin();
    if (m_idleTimer)
        m_idleTimer->start();
}

void SpellHighlighter::rehighlightNextChunk()
{
    if (!document() || m_suspended || !m_enabled)
        return;

    // ~40 blocks per tick keeps open/edit responsive on 100+ page docs.
    constexpr int kChunk = 40;
    int n = 0;
    while (m_idleBlock.isValid() && n < kChunk) {
        rehighlightBlock(m_idleBlock);
        m_idleBlock = m_idleBlock.next();
        ++n;
    }
    if (m_idleBlock.isValid() && m_idleTimer)
        m_idleTimer->start();
}

void SpellHighlighter::highlightBlock(const QString &text)
{
    if (m_suspended || !m_enabled || !SpellChecker::isAvailable())
        return;

    static const QRegularExpression wordRe(QStringLiteral("\\b[A-Za-z][A-Za-z'\\-]*\\b"));
    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString word = match.captured();
        if (SpellChecker::isMisspelled(word))
            setFormat(match.capturedStart(), match.capturedLength(), m_errorFormat);
    }
}
