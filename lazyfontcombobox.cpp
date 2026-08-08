#include "lazyfontcombobox.h"

#include <QFontDatabase>
#include <QLineEdit>

LazyFontComboBox::LazyFontComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(20);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(12);

    const QFont appFont = font();
    const QString family = appFont.family().isEmpty()
        ? QStringLiteral("PingFang SC")
        : appFont.family();
    addItem(family);
    setCurrentText(family);

    connect(this, &QComboBox::activated, this, [this](int) { emitFontChanged(); });
    if (lineEdit()) {
        connect(lineEdit(), &QLineEdit::editingFinished, this, [this]() { emitFontChanged(); });
    }
}

void LazyFontComboBox::setCurrentFont(const QFont &font)
{
    const QString family = font.family();
    if (family.isEmpty())
        return;

    m_blockEmit = true;
    if (m_populated) {
        const int idx = findText(family, Qt::MatchFixedString);
        if (idx >= 0)
            setCurrentIndex(idx);
        else
            setCurrentText(family);
    } else {
        if (count() == 0)
            addItem(family);
        else
            setItemText(0, family);
        setCurrentText(family);
    }
    m_blockEmit = false;
}

QFont LazyFontComboBox::currentFont() const
{
    QFont f = font();
    const QString family = currentText().trimmed();
    if (!family.isEmpty())
        f.setFamily(family);
    return f;
}

void LazyFontComboBox::showPopup()
{
    ensurePopulated();
    QComboBox::showPopup();
}

void LazyFontComboBox::ensurePopulated()
{
    if (m_populated)
        return;
    m_populated = true;

    const QString keep = currentText();
    m_blockEmit = true;
    clear();
    const QStringList families = QFontDatabase::families();
    addItems(families);
    const int idx = findText(keep, Qt::MatchFixedString);
    if (idx >= 0)
        setCurrentIndex(idx);
    else if (!keep.isEmpty())
        setCurrentText(keep);
    m_blockEmit = false;
}

void LazyFontComboBox::emitFontChanged()
{
    if (m_blockEmit)
        return;
    emit currentFontChanged(currentFont());
}
