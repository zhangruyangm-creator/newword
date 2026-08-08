#ifndef FORMULADIALOG_H
#define FORMULADIALOG_H

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QComboBox;
class QDialogButtonBox;
class QTimer;

class FormulaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FormulaDialog(const QString &initialLatex = QString(),
                           qreal initialPointSize = 18.0,
                           QWidget *parent = nullptr);

    QString latex() const;
    qreal pointSize() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();
    void validateAccept();

private:
    QPlainTextEdit *m_input = nullptr;
    QLabel *m_preview = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QTimer *m_previewTimer = nullptr;
};

#endif // FORMULADIALOG_H
