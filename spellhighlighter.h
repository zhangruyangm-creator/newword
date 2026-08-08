#ifndef SPELLHIGHLIGHTER_H
#define SPELLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTimer>

class SpellHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit SpellHighlighter(QTextDocument *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    //! While suspended, highlightBlock is a no-op (no full rehighlight).
    //! Used during large file load to avoid spell-checking every inserted block.
    void setSuspended(bool suspended);
    bool isSuspended() const { return m_suspended; }

    //! Re-enable highlighting in small idle chunks (keeps UI responsive on large docs).
    void rehighlightIdle();

protected:
    void highlightBlock(const QString &text) override;

private:
    void rehighlightNextChunk();

    bool m_enabled = true;
    bool m_suspended = false;
    QTextCharFormat m_errorFormat;
    QTextBlock m_idleBlock;
    QTimer *m_idleTimer = nullptr;
};

#endif // SPELLHIGHLIGHTER_H
