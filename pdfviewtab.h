#ifndef PDFVIEWTAB_H
#define PDFVIEWTAB_H

#include <QWidget>

/** Read-only PDF viewer tab backed by macOS PDFKit. */
class PdfViewTab : public QWidget
{
    Q_OBJECT
public:
    explicit PdfViewTab(QWidget *parent = nullptr);
    ~PdfViewTab() override;

    [[nodiscard]] bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] int currentPage() const; // 0-based
    [[nodiscard]] int zoomPercent() const { return m_zoomPercent; }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void setZoomPercent(int percent);
    void goToPreviousPage();
    void goToNextPage();

signals:
    void zoomChanged(int percent);
    void pageChanged(int pageIndex, int pageCount);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void ensureNativeView();
    void layoutNativeView();
    void applyZoom();
    void syncPageSignal();

    QString m_filePath;
    int m_zoomPercent = 100;
    void *m_pdfView = nullptr; // PDFView*
};

#endif // PDFVIEWTAB_H
