#ifndef TABLEDIALOGS_H
#define TABLEDIALOGS_H

#include <QDialog>
#include <QTextTableFormat>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

class InsertTableDialog : public QDialog
{
    Q_OBJECT
public:
    explicit InsertTableDialog(QWidget *parent = nullptr);

    int rowCount() const;
    int columnCount() const;
    bool withHeaderRow() const;
    QTextTableFormat tableFormat() const;

private:
    QSpinBox *m_rows = nullptr;
    QSpinBox *m_cols = nullptr;
    QCheckBox *m_header = nullptr;
    QDoubleSpinBox *m_border = nullptr;
    QDoubleSpinBox *m_padding = nullptr;
    QComboBox *m_width = nullptr;
};

class TablePropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TablePropertiesDialog(const QTextTableFormat &format, QWidget *parent = nullptr);

    QTextTableFormat tableFormat() const;

private:
    QDoubleSpinBox *m_border = nullptr;
    QDoubleSpinBox *m_padding = nullptr;
    QComboBox *m_alignment = nullptr;
    QComboBox *m_width = nullptr;
};

class ColumnWidthsDialog : public QDialog
{
    Q_OBJECT
public:
    ColumnWidthsDialog(const QVector<qreal> &percents, int highlightColumn = -1,
                       QWidget *parent = nullptr);

    [[nodiscard]] QVector<qreal> percents() const;

private slots:
    void distributeEvenly();
    void normalizeTo100();
    void updateSumLabel();

private:
    QVector<QDoubleSpinBox *> m_spins;
    QLabel *m_sumLabel = nullptr;
};

class RowHeightDialog : public QDialog
{
    Q_OBJECT
public:
    RowHeightDialog(qreal currentHeightPt, QWidget *parent = nullptr);

    [[nodiscard]] qreal heightPt() const; //!< 0 = auto
    [[nodiscard]] bool applyToAllRows() const;

private:
    QDoubleSpinBox *m_height = nullptr;
    QCheckBox *m_allRows = nullptr;
    QCheckBox *m_autoHeight = nullptr;
};

#endif // TABLEDIALOGS_H
