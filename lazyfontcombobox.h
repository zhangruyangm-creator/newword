#ifndef LAZYFONTCOMBOBOX_H
#define LAZYFONTCOMBOBOX_H

#include <QComboBox>
#include <QFont>

/**
 * Font family picker that defers QFontDatabase::families() until the popup opens.
 * QFontComboBox enumerates every installed font in its constructor — expensive on macOS
 * and the wrong trade-off for a “fast / light” cold start.
 */
class LazyFontComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit LazyFontComboBox(QWidget *parent = nullptr);

    void setCurrentFont(const QFont &font);
    [[nodiscard]] QFont currentFont() const;

signals:
    void currentFontChanged(const QFont &font);

protected:
    void showPopup() override;

private:
    void ensurePopulated();
    void emitFontChanged();

    bool m_populated = false;
    bool m_blockEmit = false;
};

#endif // LAZYFONTCOMBOBOX_H
