#include "headerfooterdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

HeaderFooterDialog::HeaderFooterDialog(const HeaderFooterSettings &settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("页眉和页脚"));
    resize(480, 420);

    m_headerEdit = new QLineEdit(settings.header, this);
    m_footerEdit = new QLineEdit(settings.footer, this);

    m_pageNumber = new QCheckBox(tr("显示页码"), this);
    m_pageNumber->setChecked(settings.showPageNumber);

    m_pageNumberFormat = new QComboBox(this);
    m_pageNumberFormat->addItem(tr("第 N 页"),
                                int(HeaderFooterSettings::PageNumberFormat::ChinesePage));
    m_pageNumberFormat->addItem(tr("第 N 页 / 共 M 页"),
                                int(HeaderFooterSettings::PageNumberFormat::ChinesePageOf));
    m_pageNumberFormat->addItem(tr("N"), int(HeaderFooterSettings::PageNumberFormat::Number));
    m_pageNumberFormat->addItem(tr("N / M"),
                                int(HeaderFooterSettings::PageNumberFormat::NumberSlash));
    m_pageNumberFormat->addItem(tr("- N -"),
                                int(HeaderFooterSettings::PageNumberFormat::DashNumber));
    const int fmtIdx =
        m_pageNumberFormat->findData(int(settings.pageNumberFormat));
    m_pageNumberFormat->setCurrentIndex(fmtIdx >= 0 ? fmtIdx : 0);
    m_pageNumberFormat->setEnabled(settings.showPageNumber);

    m_differentFirst = new QCheckBox(tr("首页不同"), this);
    m_differentFirst->setChecked(settings.differentFirstPage);
    m_firstHeader = new QLineEdit(settings.firstHeader, this);
    m_firstFooter = new QLineEdit(settings.firstFooter, this);
    m_firstPageFields = new QWidget(this);
    auto *firstForm = new QFormLayout(m_firstPageFields);
    firstForm->setContentsMargins(16, 0, 0, 0);
    firstForm->addRow(tr("首页页眉:"), m_firstHeader);
    firstForm->addRow(tr("首页页脚:"), m_firstFooter);

    m_differentOddEven = new QCheckBox(tr("奇偶页不同"), this);
    m_differentOddEven->setChecked(settings.differentOddEven);
    m_evenHeader = new QLineEdit(settings.evenHeader, this);
    m_evenFooter = new QLineEdit(settings.evenFooter, this);
    m_oddEvenFields = new QWidget(this);
    auto *oddForm = new QFormLayout(m_oddEvenFields);
    oddForm->setContentsMargins(16, 0, 0, 0);
    oddForm->addRow(tr("偶数页页眉:"), m_evenHeader);
    oddForm->addRow(tr("偶数页页脚:"), m_evenFooter);

    auto *hint = new QLabel(
        tr("页眉/页脚会显示在分页预览、打印与 PDF 导出中。上方为默认（奇数页）内容。"),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #666;"));

    auto *form = new QFormLayout;
    form->addRow(tr("页眉:"), m_headerEdit);
    form->addRow(tr("页脚:"), m_footerEdit);
    form->addRow(tr("页码格式:"), m_pageNumberFormat);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addWidget(m_pageNumber);
    layout->addWidget(m_differentFirst);
    layout->addWidget(m_firstPageFields);
    layout->addWidget(m_differentOddEven);
    layout->addWidget(m_oddEvenFields);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(m_pageNumber, &QCheckBox::toggled, m_pageNumberFormat, &QWidget::setEnabled);
    connect(m_differentFirst, &QCheckBox::toggled, this, &HeaderFooterDialog::updateFieldVisibility);
    connect(m_differentOddEven, &QCheckBox::toggled, this, &HeaderFooterDialog::updateFieldVisibility);
    updateFieldVisibility();
}

void HeaderFooterDialog::updateFieldVisibility()
{
    m_firstPageFields->setVisible(m_differentFirst->isChecked());
    m_oddEvenFields->setVisible(m_differentOddEven->isChecked());
}

HeaderFooterSettings HeaderFooterDialog::settings() const
{
    HeaderFooterSettings s;
    s.header = m_headerEdit->text();
    s.footer = m_footerEdit->text();
    s.showPageNumber = m_pageNumber->isChecked();
    s.pageNumberFormat = HeaderFooterSettings::PageNumberFormat(
        m_pageNumberFormat->currentData().toInt());
    s.differentFirstPage = m_differentFirst->isChecked();
    s.firstHeader = m_firstHeader->text();
    s.firstFooter = m_firstFooter->text();
    s.differentOddEven = m_differentOddEven->isChecked();
    s.evenHeader = m_evenHeader->text();
    s.evenFooter = m_evenFooter->text();
    return s;
}
