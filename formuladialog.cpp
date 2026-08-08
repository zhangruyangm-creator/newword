#include "formuladialog.h"
#include "formularenderer.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

FormulaDialog::FormulaDialog(const QString &initialLatex, qreal initialPointSize, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(initialLatex.isEmpty() ? tr("插入公式") : tr("编辑公式"));
    resize(520, 420);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("输入 LaTeX 公式，例如：E=mc^2 或 \\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}"));
    m_input->setPlainText(initialLatex.isEmpty()
                              ? QStringLiteral("E=mc^2")
                              : FormulaRenderer::stripMathDelimiters(initialLatex));
    m_input->setTabChangesFocus(true);
    m_input->setMinimumHeight(100);

    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->addItem(tr("小"), 14.0);
    m_sizeCombo->addItem(tr("中"), 18.0);
    m_sizeCombo->addItem(tr("大"), 24.0);
    m_sizeCombo->addItem(tr("特大"), 32.0);
    int sizeIndex = 1;
    qreal bestDelta = qAbs(initialPointSize - 18.0);
    for (int i = 0; i < m_sizeCombo->count(); ++i) {
        const qreal delta = qAbs(m_sizeCombo->itemData(i).toDouble() - initialPointSize);
        if (delta < bestDelta) {
            bestDelta = delta;
            sizeIndex = i;
        }
    }
    m_sizeCombo->setCurrentIndex(sizeIndex);

    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(100);
    m_preview->setStyleSheet(QStringLiteral(
        "QLabel { background: #FAFAFA; border: 1px solid #D0D0D0; border-radius: 4px; }"));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(m_preview);
    scroll->setMinimumHeight(120);

    auto *examples = new QLabel(
        tr("常用：a_n  a^{2}  \\frac{a}{b}  \\sqrt{x}  \\alpha\\beta  \\sum_{i=1}^{n}  "
           "\\int_{a}^{b}  \\pm\\times\\leq"),
        this);
    examples->setWordWrap(true);
    examples->setStyleSheet(QStringLiteral("color: #666;"));

    auto *form = new QFormLayout;
    form->addRow(tr("字号"), m_sizeCombo);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &FormulaDialog::validateAccept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("LaTeX："), this));
    layout->addWidget(m_input);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("预览："), this));
    layout->addWidget(scroll, 1);
    layout->addWidget(examples);
    layout->addWidget(m_buttons);

    connect(m_input, &QPlainTextEdit::textChanged, this, &FormulaDialog::schedulePreviewUpdate);
    connect(m_sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormulaDialog::schedulePreviewUpdate);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(120);
    connect(m_previewTimer, &QTimer::timeout, this, &FormulaDialog::updatePreview);
    updatePreview();
}

QString FormulaDialog::latex() const
{
    return FormulaRenderer::stripMathDelimiters(m_input->toPlainText());
}

qreal FormulaDialog::pointSize() const
{
    return m_sizeCombo->currentData().toDouble();
}

void FormulaDialog::schedulePreviewUpdate()
{
    if (m_previewTimer)
        m_previewTimer->start();
}

void FormulaDialog::updatePreview()
{
    const QString src = latex();
    if (src.trimmed().isEmpty()) {
        m_preview->setText(tr("（空）"));
        m_preview->setPixmap({});
        return;
    }
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
    QImage image = FormulaRenderer::render(src, pointSize(), dpr);
    if (image.isNull()) {
        m_preview->setText(tr("预览失败"));
        m_preview->setPixmap({});
        return;
    }
    m_preview->setPixmap(QPixmap::fromImage(image));
    m_preview->setText(QString());
}

void FormulaDialog::validateAccept()
{
    if (latex().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("插入公式"), tr("请输入公式内容。"));
        return;
    }
    accept();
}
