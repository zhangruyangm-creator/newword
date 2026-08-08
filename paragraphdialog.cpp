#include "paragraphdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

namespace {
QDoubleSpinBox *makePtSpin(QWidget *parent, qreal value, qreal min, qreal max)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(1);
    spin->setSuffix(QStringLiteral(" pt"));
    spin->setSingleStep(1.0);
    spin->setValue(value);
    return spin;
}
} // namespace

ParagraphDialog::ParagraphDialog(const QTextBlockFormat &format, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("段落"));
    resize(380, 360);

    m_alignBox = new QComboBox(this);
    m_alignBox->addItem(tr("左对齐"), int(Qt::AlignLeft));
    m_alignBox->addItem(tr("居中"), int(Qt::AlignHCenter));
    m_alignBox->addItem(tr("右对齐"), int(Qt::AlignRight));
    m_alignBox->addItem(tr("两端对齐"), int(Qt::AlignJustify));

    const Qt::Alignment align = format.alignment();
    if (align & Qt::AlignHCenter)
        m_alignBox->setCurrentIndex(1);
    else if (align & Qt::AlignRight)
        m_alignBox->setCurrentIndex(2);
    else if (align & Qt::AlignJustify)
        m_alignBox->setCurrentIndex(3);
    else
        m_alignBox->setCurrentIndex(0);

    // Qt indent is in document units roughly related to tab stops; expose as pt-like values.
    m_leftIndent = makePtSpin(this, format.leftMargin(), 0, 200);
    m_rightIndent = makePtSpin(this, format.rightMargin(), 0, 200);
    m_firstLine = makePtSpin(this, format.textIndent(), -100, 200);
    m_spaceBefore = makePtSpin(this, format.topMargin(), 0, 100);
    m_spaceAfter = makePtSpin(this, format.bottomMargin(), 0, 100);

    m_lineSpacing = new QComboBox(this);
    m_lineSpacing->addItem(tr("单倍行距"), 1.0);
    m_lineSpacing->addItem(tr("1.15 倍"), 1.15);
    m_lineSpacing->addItem(tr("1.5 倍"), 1.5);
    m_lineSpacing->addItem(tr("双倍行距"), 2.0);
    m_lineSpacing->addItem(tr("2.5 倍"), 2.5);
    m_lineSpacing->addItem(tr("3 倍"), 3.0);

    qreal factor = 1.15;
    if (format.lineHeightType() == QTextBlockFormat::ProportionalHeight)
        factor = format.lineHeight() / 100.0;
    int idx = m_lineSpacing->findData(factor);
    if (idx < 0) {
        // pick closest
        idx = 1;
        qreal best = 999;
        for (int i = 0; i < m_lineSpacing->count(); ++i) {
            const qreal d = qAbs(m_lineSpacing->itemData(i).toDouble() - factor);
            if (d < best) {
                best = d;
                idx = i;
            }
        }
    }
    m_lineSpacing->setCurrentIndex(idx);

    auto *indentGroup = new QGroupBox(tr("缩进"), this);
    auto *indentForm = new QFormLayout(indentGroup);
    indentForm->addRow(tr("左侧"), m_leftIndent);
    indentForm->addRow(tr("右侧"), m_rightIndent);
    indentForm->addRow(tr("首行"), m_firstLine);

    auto *spacingGroup = new QGroupBox(tr("间距"), this);
    auto *spacingForm = new QFormLayout(spacingGroup);
    spacingForm->addRow(tr("段前"), m_spaceBefore);
    spacingForm->addRow(tr("段后"), m_spaceAfter);
    spacingForm->addRow(tr("行距"), m_lineSpacing);

    auto *general = new QFormLayout;
    general->addRow(tr("对齐方式"), m_alignBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(general);
    root->addWidget(indentGroup);
    root->addWidget(spacingGroup);
    root->addWidget(buttons);
}

QTextBlockFormat ParagraphDialog::format() const
{
    QTextBlockFormat fmt;
    fmt.setAlignment(Qt::Alignment(m_alignBox->currentData().toInt()));
    fmt.setLeftMargin(m_leftIndent->value());
    fmt.setRightMargin(m_rightIndent->value());
    fmt.setTextIndent(m_firstLine->value());
    fmt.setTopMargin(m_spaceBefore->value());
    fmt.setBottomMargin(m_spaceAfter->value());
    fmt.setLineHeight(m_lineSpacing->currentData().toDouble() * 100.0,
                      QTextBlockFormat::ProportionalHeight);
    return fmt;
}
