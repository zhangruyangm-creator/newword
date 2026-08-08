#ifndef PAGESETUPDIALOG_H
#define PAGESETUPDIALOG_H

#include "pagelayout.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

class PageSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PageSetupDialog(const PageLayoutSettings &settings, QWidget *parent = nullptr);

    PageLayoutSettings settings() const;

private slots:
    void onPaperChanged();
    void applyMarginPreset(int index);
    void updatePreviewLabel();

private:
    void loadFromSettings(const PageLayoutSettings &settings);

    QComboBox *m_paperBox = nullptr;
    QComboBox *m_orientationBox = nullptr;
    QDoubleSpinBox *m_customWidth = nullptr;
    QDoubleSpinBox *m_customHeight = nullptr;

    QComboBox *m_marginPreset = nullptr;
    QDoubleSpinBox *m_marginLeft = nullptr;
    QDoubleSpinBox *m_marginRight = nullptr;
    QDoubleSpinBox *m_marginTop = nullptr;
    QDoubleSpinBox *m_marginBottom = nullptr;

    QSpinBox *m_columns = nullptr;
    QDoubleSpinBox *m_columnSpacing = nullptr;
    QDoubleSpinBox *m_headerDistance = nullptr;
    QDoubleSpinBox *m_footerDistance = nullptr;

    QCheckBox *m_pageBorder = nullptr;
    QDoubleSpinBox *m_borderWidth = nullptr;

    QLabel *m_summary = nullptr;
};

#endif // PAGESETUPDIALOG_H
