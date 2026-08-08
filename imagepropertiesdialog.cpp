#include "imagepropertiesdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

ImagePropertiesDialog::ImagePropertiesDialog(const QTextImageFormat &format,
                                             const QImage &image,
                                             QWidget *parent)
    : QDialog(parent)
    , m_format(format)
    , m_image(image)
{
    setWindowTitle(tr("图片属性"));
    setModal(true);

    const qreal w = format.width() > 0 ? format.width()
                                       : (image.isNull() ? 320.0 : image.width());
    const qreal h = format.height() > 0 ? format.height()
                                        : (image.isNull() ? 240.0 : image.height());

    m_width = new QDoubleSpinBox(this);
    m_width->setRange(24, 2000);
    m_width->setSuffix(tr(" px"));
    m_width->setValue(w);

    m_height = new QDoubleSpinBox(this);
    m_height->setRange(24, 2000);
    m_height->setSuffix(tr(" px"));
    m_height->setValue(h);

    m_wrap = new QComboBox(this);
    m_wrap->addItem(tr("嵌入行内"), int(ImageProps::Wrap::Inline));
    m_wrap->addItem(tr("单独成段（推荐）"), int(ImageProps::Wrap::Block));
    m_wrap->addItem(tr("四周型 · 左浮（预览/PDF）"), int(ImageProps::Wrap::FloatLeft));
    m_wrap->addItem(tr("四周型 · 右浮（预览/PDF）"), int(ImageProps::Wrap::FloatRight));
    m_wrap->setCurrentIndex(m_wrap->findData(int(ImageProps::wrapOf(format))));

    m_align = new QComboBox(this);
    m_align->addItem(tr("左对齐"), int(Qt::AlignLeft));
    m_align->addItem(tr("居中"), int(Qt::AlignHCenter));
    m_align->addItem(tr("右对齐"), int(Qt::AlignRight));
    m_align->setCurrentIndex(m_align->findData(int(ImageProps::alignOf(format))));

    auto *hint = new QLabel(
        tr("四周型绕排在分页预览与 PDF 中生效；活页编辑区仍以段落近似显示。"), this);
    hint->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(tr("宽度"), m_width);
    form->addRow(tr("高度"), m_height);
    form->addRow(tr("环绕"), m_wrap);
    form->addRow(tr("对齐"), m_align);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const qreal aspect = (h > 0.01) ? (w / h) : 1.0;
    connect(m_width, &QDoubleSpinBox::valueChanged, this, [this, aspect](double nw) {
        if (!m_lockAspect)
            return;
        QSignalBlocker b(m_height);
        m_height->setValue(nw / aspect);
    });
    connect(m_height, &QDoubleSpinBox::valueChanged, this, [this, aspect](double nh) {
        if (!m_lockAspect)
            return;
        QSignalBlocker b(m_width);
        m_width->setValue(nh * aspect);
    });

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(hint);
    root->addWidget(buttons);
}

QTextImageFormat ImagePropertiesDialog::format() const
{
    QTextImageFormat fmt = m_format;
    fmt.setWidth(m_width->value());
    fmt.setHeight(m_height->value());
    ImageProps::setWrap(&fmt, ImageProps::Wrap(m_wrap->currentData().toInt()));
    ImageProps::setAlign(&fmt, Qt::Alignment(m_align->currentData().toInt()));
    return fmt;
}
