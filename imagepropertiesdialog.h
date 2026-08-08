#ifndef IMAGEPROPERTIESDIALOG_H
#define IMAGEPROPERTIESDIALOG_H

#include "imageprops.h"

#include <QDialog>
#include <QTextImageFormat>

class QComboBox;
class QDoubleSpinBox;

class ImagePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImagePropertiesDialog(const QTextImageFormat &format,
                                   const QImage &image,
                                   QWidget *parent = nullptr);

    [[nodiscard]] QTextImageFormat format() const;

private:
    QTextImageFormat m_format;
    QImage m_image;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_height = nullptr;
    QComboBox *m_wrap = nullptr;
    QComboBox *m_align = nullptr;
    bool m_lockAspect = true;
};

#endif // IMAGEPROPERTIESDIALOG_H
