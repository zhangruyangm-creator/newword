#ifndef ZIPARCHIVE_H
#define ZIPARCHIVE_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

/** Minimal ZIP reader/writer for OOXML (DOCX). Backed by minizip-ng. */
class ZipReader
{
public:
    explicit ZipReader(const QString &filePath);

    [[nodiscard]] bool isValid() const { return m_valid; }
    [[nodiscard]] QStringList fileNames() const;
    [[nodiscard]] QByteArray fileData(const QString &fileName) const;

private:
    bool m_valid = false;
    QHash<QString, QByteArray> m_entries; //!< entry name → decompressed data
};

class ZipWriter
{
public:
    explicit ZipWriter(const QString &filePath);
    ~ZipWriter();

    [[nodiscard]] bool isValid() const { return m_valid; }
    void addFile(const QString &fileName, const QByteArray &data);
    [[nodiscard]] bool close();

private:
    bool m_valid = false;
    bool m_closed = false;
    QString m_filePath;
    void *m_zip = nullptr; //!< minizip-ng zipFile handle
};

#endif // ZIPARCHIVE_H
