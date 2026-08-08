#include "tabledialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

InsertTableDialog::InsertTableDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("插入表格"));
    resize(360, 280);

    m_rows = new QSpinBox(this);
    m_rows->setRange(1, 50);
    m_rows->setValue(3);
    m_cols = new QSpinBox(this);
    m_cols->setRange(1, 20);
    m_cols->setValue(3);
    m_header = new QCheckBox(tr("首行作为表头"), this);
    m_header->setChecked(true);

    m_border = new QDoubleSpinBox(this);
    m_border->setRange(0, 8);
    m_border->setDecimals(1);
    m_border->setSingleStep(0.5);
    m_border->setValue(1.0);
    m_border->setSuffix(QStringLiteral(" pt"));

    m_padding = new QDoubleSpinBox(this);
    m_padding->setRange(0, 24);
    m_padding->setDecimals(1);
    m_padding->setValue(6.0);
    m_padding->setSuffix(QStringLiteral(" pt"));

    m_width = new QComboBox(this);
    m_width->addItem(tr("100% 页宽"), 100);
    m_width->addItem(tr("80% 页宽"), 80);
    m_width->addItem(tr("60% 页宽"), 60);
    m_width->addItem(tr("自适应"), 0);

    auto *form = new QFormLayout;
    form->addRow(tr("行数"), m_rows);
    form->addRow(tr("列数"), m_cols);
    form->addRow(QString(), m_header);
    form->addRow(tr("边框"), m_border);
    form->addRow(tr("单元格边距"), m_padding);
    form->addRow(tr("表格宽度"), m_width);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int InsertTableDialog::rowCount() const { return m_rows->value(); }
int InsertTableDialog::columnCount() const { return m_cols->value(); }
bool InsertTableDialog::withHeaderRow() const { return m_header->isChecked(); }

QTextTableFormat InsertTableDialog::tableFormat() const
{
    QTextTableFormat fmt;
    fmt.setBorderStyle(m_border->value() > 0 ? QTextFrameFormat::BorderStyle_Solid
                                             : QTextFrameFormat::BorderStyle_None);
    fmt.setBorder(m_border->value());
    fmt.setCellPadding(m_padding->value());
    fmt.setCellSpacing(0);
    fmt.setAlignment(Qt::AlignLeft);
    const int pct = m_width->currentData().toInt();
    if (pct > 0)
        fmt.setWidth(QTextLength(QTextLength::PercentageLength, pct));
    return fmt;
}

TablePropertiesDialog::TablePropertiesDialog(const QTextTableFormat &format, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("表格属性"));
    resize(360, 260);

    m_border = new QDoubleSpinBox(this);
    m_border->setRange(0, 8);
    m_border->setDecimals(1);
    m_border->setSingleStep(0.5);
    m_border->setValue(format.border());
    m_border->setSuffix(QStringLiteral(" pt"));

    m_padding = new QDoubleSpinBox(this);
    m_padding->setRange(0, 24);
    m_padding->setDecimals(1);
    m_padding->setValue(format.cellPadding());
    m_padding->setSuffix(QStringLiteral(" pt"));

    m_alignment = new QComboBox(this);
    m_alignment->addItem(tr("左对齐"), int(Qt::AlignLeft));
    m_alignment->addItem(tr("居中"), int(Qt::AlignHCenter));
    m_alignment->addItem(tr("右对齐"), int(Qt::AlignRight));
    const Qt::Alignment align = format.alignment();
    if (align & Qt::AlignHCenter)
        m_alignment->setCurrentIndex(1);
    else if (align & Qt::AlignRight)
        m_alignment->setCurrentIndex(2);
    else
        m_alignment->setCurrentIndex(0);

    m_width = new QComboBox(this);
    m_width->addItem(tr("100% 页宽"), 100);
    m_width->addItem(tr("80% 页宽"), 80);
    m_width->addItem(tr("60% 页宽"), 60);
    m_width->addItem(tr("自适应"), 0);
    int pct = 100;
    if (format.width().type() == QTextLength::PercentageLength)
        pct = qRound(format.width().value(100));
    else
        pct = 0;
    const int idx = m_width->findData(pct);
    m_width->setCurrentIndex(idx >= 0 ? idx : 0);

    auto *form = new QFormLayout;
    form->addRow(tr("边框"), m_border);
    form->addRow(tr("单元格边距"), m_padding);
    form->addRow(tr("对齐"), m_alignment);
    form->addRow(tr("表格宽度"), m_width);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QTextTableFormat TablePropertiesDialog::tableFormat() const
{
    QTextTableFormat fmt;
    fmt.setBorderStyle(m_border->value() > 0 ? QTextFrameFormat::BorderStyle_Solid
                                             : QTextFrameFormat::BorderStyle_None);
    fmt.setBorder(m_border->value());
    fmt.setCellPadding(m_padding->value());
    fmt.setCellSpacing(0);
    fmt.setAlignment(Qt::Alignment(m_alignment->currentData().toInt()));
    const int pct = m_width->currentData().toInt();
    if (pct > 0)
        fmt.setWidth(QTextLength(QTextLength::PercentageLength, pct));
    else
        fmt.setWidth(QTextLength());
    return fmt;
}

ColumnWidthsDialog::ColumnWidthsDialog(const QVector<qreal> &percents, int highlightColumn,
                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("列宽"));
    resize(380, qMin(480, 120 + percents.size() * 36));

    auto *formHost = new QWidget(this);
    auto *form = new QFormLayout(formHost);
    m_spins.reserve(percents.size());
    for (int i = 0; i < percents.size(); ++i) {
        auto *spin = new QDoubleSpinBox(formHost);
        spin->setRange(1.0, 99.0);
        spin->setDecimals(1);
        spin->setSingleStep(1.0);
        spin->setSuffix(QStringLiteral(" %"));
        spin->setValue(percents.value(i, 100.0 / qMax(1, percents.size())));
        if (i == highlightColumn) {
            QFont f = spin->font();
            f.setBold(true);
            spin->setFont(f);
        }
        form->addRow(tr("第 %1 列").arg(i + 1), spin);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                &ColumnWidthsDialog::updateSumLabel);
        m_spins.append(spin);
    }

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(formHost);
    scroll->setFrameShape(QFrame::NoFrame);

    m_sumLabel = new QLabel(this);
    auto *evenBtn = new QPushButton(tr("平均分配"), this);
    auto *normBtn = new QPushButton(tr("归一化到 100%"), this);
    connect(evenBtn, &QPushButton::clicked, this, &ColumnWidthsDialog::distributeEvenly);
    connect(normBtn, &QPushButton::clicked, this, &ColumnWidthsDialog::normalizeTo100);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(evenBtn);
    btnRow->addWidget(normBtn);
    btnRow->addStretch();

    auto *hint = new QLabel(tr("列宽为相对百分比；确定时会自动归一化到 100%。"), this);
    hint->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(scroll, 1);
    layout->addWidget(m_sumLabel);
    layout->addLayout(btnRow);
    layout->addWidget(buttons);

    updateSumLabel();
}

QVector<qreal> ColumnWidthsDialog::percents() const
{
    QVector<qreal> out;
    out.reserve(m_spins.size());
    for (const QDoubleSpinBox *spin : m_spins)
        out.append(spin->value());
    return out;
}

void ColumnWidthsDialog::distributeEvenly()
{
    if (m_spins.isEmpty())
        return;
    const qreal even = 100.0 / m_spins.size();
    for (QDoubleSpinBox *spin : m_spins)
        spin->setValue(even);
}

void ColumnWidthsDialog::normalizeTo100()
{
    qreal sum = 0;
    for (const QDoubleSpinBox *spin : m_spins)
        sum += spin->value();
    if (sum <= 0)
        return;
    for (QDoubleSpinBox *spin : m_spins)
        spin->setValue(spin->value() * 100.0 / sum);
}

void ColumnWidthsDialog::updateSumLabel()
{
    qreal sum = 0;
    for (const QDoubleSpinBox *spin : m_spins)
        sum += spin->value();
    m_sumLabel->setText(tr("合计：%1%").arg(sum, 0, 'f', 1));
}

RowHeightDialog::RowHeightDialog(qreal currentHeightPt, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("行高"));
    resize(360, 220);

    m_autoHeight = new QCheckBox(tr("自动行高（随内容）"), this);
    m_autoHeight->setChecked(currentHeightPt <= 0.5);

    m_height = new QDoubleSpinBox(this);
    m_height->setRange(12.0, 240.0);
    m_height->setDecimals(1);
    m_height->setSingleStep(2.0);
    m_height->setSuffix(QStringLiteral(" pt"));
    m_height->setValue(currentHeightPt > 0.5 ? currentHeightPt : 28.0);
    m_height->setEnabled(!m_autoHeight->isChecked());

    auto *cmHint = new QLabel(this);
    auto updateCm = [this, cmHint]() {
        const qreal cm = m_height->value() * 25.4 / 72.0;
        cmHint->setText(tr("约 %1 cm（最小高度，内容更高时仍会撑开）").arg(cm, 0, 'f', 2));
    };
    connect(m_height, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updateCm);
    updateCm();

    m_allRows = new QCheckBox(tr("应用到整张表的所有行"), this);

    connect(m_autoHeight, &QCheckBox::toggled, this, [this](bool on) {
        m_height->setEnabled(!on);
    });

    auto *form = new QFormLayout;
    form->addRow(QString(), m_autoHeight);
    form->addRow(tr("最小行高"), m_height);
    form->addRow(QString(), cmHint);
    form->addRow(QString(), m_allRows);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

qreal RowHeightDialog::heightPt() const
{
    return m_autoHeight->isChecked() ? 0.0 : m_height->value();
}

bool RowHeightDialog::applyToAllRows() const
{
    return m_allRows->isChecked();
}
