#ifndef ZIPARCHIVE_H
#define ZIPARCHIVE_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

/** Minimal ZIP reader/writer for OOXML (DOCX). Uses zlib; no Qt private APIs. */
class ZipReader
{
public:
    explicit ZipReader(const QString &filePath);

    [[nodiscard]] bool isValid() const { return m_valid; }
    [[nodiscard]] QStringList fileNames() const;
    [[nodiscard]] QByteArray fileData(const QString &fileName) const;

private:
    struct Entry {
        quint32 localHeaderOffset = 0;
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint16 method = 0;
    };

    bool m_valid = false;
    QByteArray m_bytes;
    QHash<QString, Entry> m_entries;
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
    struct Written {
        QString name;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint32 localHeaderOffset = 0;
        quint16 method = 0;
        QByteArray compressed;
    };

    bool m_valid = false;
    bool m_closed = false;
    QString m_filePath;
    QList<Written> m_files;
};

#endif // ZIPARCHIVE_H
