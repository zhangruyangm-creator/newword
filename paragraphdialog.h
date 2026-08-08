#ifndef PARAGRAPHDIALOG_H
#define PARAGRAPHDIALOG_H

#include <QDialog>
#include <QTextBlockFormat>

class QComboBox;
class QDoubleSpinBox;

class ParagraphDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParagraphDialog(const QTextBlockFormat &format, QWidget *parent = nullptr);

    QTextBlockFormat format() const;

private:
    QComboBox *m_alignBox = nullptr;
    QDoubleSpinBox *m_leftIndent = nullptr;
    QDoubleSpinBox *m_rightIndent = nullptr;
    QDoubleSpinBox *m_firstLine = nullptr;
    QDoubleSpinBox *m_spaceBefore = nullptr;
    QDoubleSpinBox *m_spaceAfter = nullptr;
    QComboBox *m_lineSpacing = nullptr;
};

#endif // PARAGRAPHDIALOG_H
