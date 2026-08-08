#include "pagesetupdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
QDoubleSpinBox *makeMmSpin(QWidget *parent, qreal value, qreal min = 0.0, qreal max = 100.0)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(1);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setSingleStep(0.5);
    spin->setValue(value);
    return spin;
}
} // namespace

PageSetupDialog::PageSetupDialog(const PageLayoutSettings &settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("页面设置"));
    resize(480, 420);

    auto *tabs = new QTabWidget(this);

    // --- Paper tab ---
    auto *paperPage = new QWidget(tabs);
    auto *paperForm = new QFormLayout(paperPage);

    m_paperBox = new QComboBox(paperPage);
    m_paperBox->addItem(tr("A4 (210 × 297 mm)"), int(PageLayoutSettings::Paper::A4));
    m_paperBox->addItem(tr("A5 (148 × 210 mm)"), int(PageLayoutSettings::Paper::A5));
    m_paperBox->addItem(tr("Letter (216 × 279 mm)"), int(PageLayoutSettings::Paper::Letter));
    m_paperBox->addItem(tr("Legal (216 × 356 mm)"), int(PageLayoutSettings::Paper::Legal));
    m_paperBox->addItem(tr("自定义"), int(PageLayoutSettings::Paper::Custom));

    m_orientationBox = new QComboBox(paperPage);
    m_orientationBox->addItem(tr("纵向"), int(PageLayoutSettings::Orientation::Portrait));
    m_orientationBox->addItem(tr("横向"), int(PageLayoutSettings::Orientation::Landscape));

    m_customWidth = makeMmSpin(paperPage, 210, 50, 600);
    m_customHeight = makeMmSpin(paperPage, 297, 50, 600);

    paperForm->addRow(tr("纸张大小"), m_paperBox);
    paperForm->addRow(tr("方向"), m_orientationBox);
    paperForm->addRow(tr("自定义宽度"), m_customWidth);
    paperForm->addRow(tr("自定义高度"), m_customHeight);
    tabs->addTab(paperPage, tr("纸张"));

    // --- Margins tab ---
    auto *marginPage = new QWidget(tabs);
    auto *marginLayout = new QVBoxLayout(marginPage);

    m_marginPreset = new QComboBox(marginPage);
    m_marginPreset->addItem(tr("自定义"));
    m_marginPreset->addItem(tr("普通"));
    m_marginPreset->addItem(tr("窄"));
    m_marginPreset->addItem(tr("适中"));
    m_marginPreset->addItem(tr("宽"));

    auto *marginForm = new QFormLayout;
    m_marginTop = makeMmSpin(marginPage, 25.4, 0, 80);
    m_marginBottom = makeMmSpin(marginPage, 25.4, 0, 80);
    m_marginLeft = makeMmSpin(marginPage, 25.4, 0, 80);
    m_marginRight = makeMmSpin(marginPage, 25.4, 0, 80);
    marginForm->addRow(tr("上"), m_marginTop);
    marginForm->addRow(tr("下"), m_marginBottom);
    marginForm->addRow(tr("左"), m_marginLeft);
    marginForm->addRow(tr("右"), m_marginRight);

    marginLayout->addWidget(new QLabel(tr("边距预设"), marginPage));
    marginLayout->addWidget(m_marginPreset);
    marginLayout->addLayout(marginForm);
    marginLayout->addStretch();
    tabs->addTab(marginPage, tr("页边距"));

    // --- Layout tab ---
    auto *layoutPage = new QWidget(tabs);
    auto *layoutForm = new QFormLayout(layoutPage);

    m_columns = new QSpinBox(layoutPage);
    m_columns->setRange(1, 3);
    m_columns->setValue(1);
    m_columns->setEnabled(false);
    m_columns->setToolTip(tr("多栏排版尚未实现，当前固定为单栏"));

    m_columnSpacing = makeMmSpin(layoutPage, 10.0, 2.0, 40.0);
    m_columnSpacing->setEnabled(false);
    m_headerDistance = makeMmSpin(layoutPage, 12.0, 0.0, 50.0);
    m_footerDistance = makeMmSpin(layoutPage, 12.0, 0.0, 50.0);

    m_pageBorder = new QCheckBox(tr("显示页边框"), layoutPage);
    m_borderWidth = makeMmSpin(layoutPage, 1.0, 0.5, 6.0);
    m_borderWidth->setSuffix(QStringLiteral(" pt"));

    layoutForm->addRow(tr("分栏数（暂不可用）"), m_columns);
    layoutForm->addRow(tr("栏间距"), m_columnSpacing);
    layoutForm->addRow(tr("页眉距边界"), m_headerDistance);
    layoutForm->addRow(tr("页脚距边界"), m_footerDistance);
    layoutForm->addRow(QString(), m_pageBorder);
    layoutForm->addRow(tr("边框线宽"), m_borderWidth);
    tabs->addTab(layoutPage, tr("版式"));

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral("color: #555; padding: 4px;"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(m_summary);
    root->addWidget(buttons);

    loadFromSettings(settings);

    connect(m_paperBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageSetupDialog::onPaperChanged);
    connect(m_orientationBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageSetupDialog::updatePreviewLabel);
    connect(m_marginPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageSetupDialog::applyMarginPreset);
    for (auto *spin : {m_customWidth, m_customHeight, m_marginLeft, m_marginRight,
                       m_marginTop, m_marginBottom, m_columnSpacing,
                       m_headerDistance, m_footerDistance, m_borderWidth}) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &PageSetupDialog::updatePreviewLabel);
    }
    connect(m_columns, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PageSetupDialog::updatePreviewLabel);
    connect(m_pageBorder, &QCheckBox::toggled, this, &PageSetupDialog::updatePreviewLabel);

    onPaperChanged();
    updatePreviewLabel();
}

void PageSetupDialog::loadFromSettings(const PageLayoutSettings &settings)
{
    m_paperBox->setCurrentIndex(m_paperBox->findData(int(settings.paper)));
    m_orientationBox->setCurrentIndex(m_orientationBox->findData(int(settings.orientation)));
    m_customWidth->setValue(settings.customWidthMm);
    m_customHeight->setValue(settings.customHeightMm);
    m_marginLeft->setValue(settings.marginsMm.left());
    m_marginRight->setValue(settings.marginsMm.right());
    m_marginTop->setValue(settings.marginsMm.top());
    m_marginBottom->setValue(settings.marginsMm.bottom());
    m_columns->setValue(settings.columnCount);
    m_columnSpacing->setValue(settings.columnSpacingMm);
    m_headerDistance->setValue(settings.headerDistanceMm);
    m_footerDistance->setValue(settings.footerDistanceMm);
    m_pageBorder->setChecked(settings.showPageBorder);
    m_borderWidth->setValue(settings.pageBorderWidthPt);
    m_marginPreset->setCurrentIndex(0);
}

PageLayoutSettings PageSetupDialog::settings() const
{
    PageLayoutSettings s;
    s.paper = PageLayoutSettings::Paper(m_paperBox->currentData().toInt());
    s.orientation = PageLayoutSettings::Orientation(m_orientationBox->currentData().toInt());
    s.customWidthMm = m_customWidth->value();
    s.customHeightMm = m_customHeight->value();
    s.marginsMm = QMarginsF(m_marginLeft->value(), m_marginTop->value(),
                            m_marginRight->value(), m_marginBottom->value());
    s.columnCount = 1; // multi-column layout not implemented yet
    s.columnSpacingMm = m_columnSpacing->value();
    s.headerDistanceMm = m_headerDistance->value();
    s.footerDistanceMm = m_footerDistance->value();
    s.showPageBorder = m_pageBorder->isChecked();
    s.pageBorderWidthPt = m_borderWidth->value();
    return s;
}

void PageSetupDialog::onPaperChanged()
{
    const bool custom = PageLayoutSettings::Paper(m_paperBox->currentData().toInt())
        == PageLayoutSettings::Paper::Custom;
    m_customWidth->setEnabled(custom);
    m_customHeight->setEnabled(custom);

    if (!custom) {
        const auto size = PageLayoutSettings::sizeForPaper(
            PageLayoutSettings::Paper(m_paperBox->currentData().toInt()));
        m_customWidth->blockSignals(true);
        m_customHeight->blockSignals(true);
        m_customWidth->setValue(size.width());
        m_customHeight->setValue(size.height());
        m_customWidth->blockSignals(false);
        m_customHeight->blockSignals(false);
    }
    updatePreviewLabel();
}

void PageSetupDialog::applyMarginPreset(int index)
{
    if (index <= 0)
        return;

    PageLayoutSettings preset;
    switch (index) {
    case 1:
        preset = PageLayoutSettings::normalMargins();
        break;
    case 2:
        preset = PageLayoutSettings::narrowMargins();
        break;
    case 3:
        preset = PageLayoutSettings::moderateMargins();
        break;
    case 4:
        preset = PageLayoutSettings::wideMargins();
        break;
    default:
        return;
    }

    m_marginLeft->setValue(preset.marginsMm.left());
    m_marginRight->setValue(preset.marginsMm.right());
    m_marginTop->setValue(preset.marginsMm.top());
    m_marginBottom->setValue(preset.marginsMm.bottom());
    updatePreviewLabel();
}

void PageSetupDialog::updatePreviewLabel()
{
    const PageLayoutSettings s = settings();
    const QSizeF size = s.pageSizeMm();
    m_summary->setText(
        tr("预览：%1 · %2 · %3 × %4 mm · 边距 上%5/下%6/左%7/右%8")
            .arg(s.paperName())
            .arg(s.orientation == PageLayoutSettings::Orientation::Landscape ? tr("横向") : tr("纵向"))
            .arg(size.width(), 0, 'f', 1)
            .arg(size.height(), 0, 'f', 1)
            .arg(s.marginsMm.top(), 0, 'f', 1)
            .arg(s.marginsMm.bottom(), 0, 'f', 1)
            .arg(s.marginsMm.left(), 0, 'f', 1)
            .arg(s.marginsMm.right(), 0, 'f', 1));
}
