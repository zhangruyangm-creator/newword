#ifndef HEADERFOOTERDIALOG_H
#define HEADERFOOTERDIALOG_H

#include "headerfootersettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QWidget;

class HeaderFooterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HeaderFooterDialog(const HeaderFooterSettings &settings, QWidget *parent = nullptr);

    HeaderFooterSettings settings() const;

private slots:
    void updateFieldVisibility();

private:
    QLineEdit *m_headerEdit = nullptr;
    QLineEdit *m_footerEdit = nullptr;
    QCheckBox *m_pageNumber = nullptr;
    QComboBox *m_pageNumberFormat = nullptr;
    QCheckBox *m_differentFirst = nullptr;
    QLineEdit *m_firstHeader = nullptr;
    QLineEdit *m_firstFooter = nullptr;
    QCheckBox *m_differentOddEven = nullptr;
    QLineEdit *m_evenHeader = nullptr;
    QLineEdit *m_evenFooter = nullptr;
    QWidget *m_firstPageFields = nullptr;
    QWidget *m_oddEvenFields = nullptr;
};

#endif // HEADERFOOTERDIALOG_H
