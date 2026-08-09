#include "ziparchive.h"

#include <unzip.h>
#include <zip.h>

#include <QIODevice>

ZipReader::ZipReader(const QString &filePath)
{
    unzFile zip = unzOpen(filePath.toUtf8().constData());
    if (!zip)
        return;

    unz_global_info global{};
    if (unzGetGlobalInfo(zip, &global) != UNZ_OK) {
        unzClose(zip);
        return;
    }

    for (unsigned long i = 0; i < global.number_entry; ++i) {
        char name[1024]{};
        unz_file_info info{};
        if (unzGetCurrentFileInfo(zip, &info, name, sizeof(name), nullptr, 0, nullptr, 0)
            != UNZ_OK)
            break;

        QByteArray data;
        if (unzOpenCurrentFile(zip) == UNZ_OK) {
            data.resize(int(info.uncompressed_size));
            int total = 0;
            while (total < data.size()) {
                const int n = unzReadCurrentFile(zip, data.data() + total, data.size() - total);
                if (n <= 0)
                    break;
                total += n;
            }
            data.resize(total);
            unzCloseCurrentFile(zip);
        }
        m_entries.insert(QString::fromUtf8(name), data);

        if (unzGoToNextFile(zip) != UNZ_OK)
            break;
    }
    unzClose(zip);
    m_valid = true;
}

QStringList ZipReader::fileNames() const
{
    return m_entries.keys();
}

QByteArray ZipReader::fileData(const QString &fileName) const
{
    return m_entries.value(fileName);
}

ZipWriter::ZipWriter(const QString &filePath)
    : m_filePath(filePath)
    , m_zip(zipOpen(filePath.toUtf8().constData(), APPEND_STATUS_CREATE))
{
    m_valid = (m_zip != nullptr);
}

ZipWriter::~ZipWriter()
{
    if (!m_closed)
        (void)close();
}

void ZipWriter::addFile(const QString &fileName, const QByteArray &data)
{
    if (!m_valid || m_closed || !m_zip)
        return;

    zip_fileinfo zfi{};
    if (zipOpenNewFileInZip(m_zip, fileName.toUtf8().constData(), &zfi, nullptr, 0, nullptr, 0,
                            nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION)
        != ZIP_OK)
        return;

    const char *bytes = data.constData();
    unsigned int remaining = static_cast<unsigned int>(data.size());
    while (remaining > 0) {
        const unsigned int chunk = qMin<unsigned int>(remaining, 1u << 20);
        if (zipWriteInFileInZip(m_zip, bytes, chunk) != ZIP_OK)
            break;
        bytes += chunk;
        remaining -= chunk;
    }
    zipCloseFileInZip(m_zip);
}

bool ZipWriter::close()
{
    if (!m_valid || m_closed)
        return false;
    m_closed = true;
    if (!m_zip)
        return false;
    const int rc = zipClose(m_zip, nullptr);
    m_zip = nullptr;
    if (rc != ZIP_OK)
        m_valid = false;
    return rc == ZIP_OK;
}
