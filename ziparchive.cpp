#include "ziparchive.h"

#include <QFile>
#include <QList>
#include <QtEndian>

#include <climits>
#include <cstddef>
#include <span>
#include <utility>
#include <zlib.h>

namespace {

quint32 crc32Of(const QByteArray &data)
{
    return static_cast<quint32>(
        ::crc32(0L, reinterpret_cast<const Bytef *>(data.constData()),
                static_cast<uInt>(data.size())));
}

QByteArray inflateRaw(const QByteArray &src, quint32 expectedSize)
{
    // Guard against malformed zips claiming absurd uncompressed sizes.
    constexpr qint64 kMaxUncompressed = 512LL * 1024 * 1024;
    qint64 initial = expectedSize ? qint64(expectedSize) : qint64(src.size()) * 4;
    initial = qBound<qint64>(0, initial, kMaxUncompressed);
    if (initial > INT_MAX)
        return {};

    QByteArray out;
    out.resize(int(initial));

    z_stream strm{};
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(src.constData()));
    strm.avail_in = static_cast<uInt>(src.size());

    // -MAX_WBITS = raw DEFLATE (ZIP)
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return {};

    int ret = Z_OK;
    while (ret == Z_OK) {
        if (strm.total_out >= static_cast<uLong>(out.size())) {
            const qint64 grown = qint64(out.size()) * 2 + 1024;
            if (grown > kMaxUncompressed || grown > INT_MAX)
                break; // inflate() below cannot finish — treat as corrupt
            out.resize(int(grown));
        }
        strm.next_out = reinterpret_cast<Bytef *>(out.data()) + strm.total_out;
        strm.avail_out = static_cast<uInt>(out.size() - int(strm.total_out));
        ret = inflate(&strm, Z_NO_FLUSH);
    }
    const uLong produced = strm.total_out;
    inflateEnd(&strm);
    if (ret != Z_STREAM_END)
        return {};
    out.resize(int(produced));
    return out;
}

QByteArray deflateRaw(const QByteArray &src)
{
    if (src.isEmpty())
        return {};

    z_stream strm{};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY)
        != Z_OK)
        return src; // fallback unused

    QByteArray out;
    out.resize(int(deflateBound(&strm, static_cast<uLong>(src.size()))));
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(src.constData()));
    strm.avail_in = static_cast<uInt>(src.size());
    strm.next_out = reinterpret_cast<Bytef *>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());

    const int ret = deflate(&strm, Z_FINISH);
    const uLong produced = strm.total_out;
    deflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        // Store uncompressed instead
        return {};
    }
    out.resize(int(produced));
    return out;
}

void appendU16(QByteArray *buf, quint16 v)
{
    char b[2];
    qToLittleEndian(v, b);
    buf->append(b, 2);
}

void appendU32(QByteArray *buf, quint32 v)
{
    char b[4];
    qToLittleEndian(v, b);
    buf->append(b, 4);
}

std::span<const uchar> asBytes(const QByteArray &b)
{
    return {reinterpret_cast<const uchar *>(b.constData()), static_cast<std::size_t>(b.size())};
}

quint16 readU16(std::span<const uchar> b, std::size_t off)
{
    return qFromLittleEndian<quint16>(b.data() + off);
}

quint32 readU32(std::span<const uchar> b, std::size_t off)
{
    return qFromLittleEndian<quint32>(b.data() + off);
}

} // namespace

ZipReader::ZipReader(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    m_bytes = file.readAll();
    if (m_bytes.size() < 22)
        return;

    const auto bytes = asBytes(m_bytes);

    // Find End of Central Directory (search last 64KB)
    const int searchFrom = qMax(0, m_bytes.size() - 65557);
    int eocd = -1;
    for (int i = m_bytes.size() - 22; i >= searchFrom; --i) {
        if (readU32(bytes, static_cast<std::size_t>(i)) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0)
        return;

    const quint16 entryCount = readU16(bytes, static_cast<std::size_t>(eocd + 10));
    [[maybe_unused]] const quint32 cdSize = readU32(bytes, static_cast<std::size_t>(eocd + 12));
    const quint32 cdOffset = readU32(bytes, static_cast<std::size_t>(eocd + 16));
    if (cdOffset > static_cast<quint32>(m_bytes.size()))
        return;

    int pos = static_cast<int>(cdOffset);
    for (quint16 i = 0; i < entryCount; ++i) {
        if (qint64(pos) + 46 > m_bytes.size()
            || readU32(bytes, static_cast<std::size_t>(pos)) != 0x02014b50)
            return;
        const quint16 method = readU16(bytes, static_cast<std::size_t>(pos + 10));
        const quint32 compSize = readU32(bytes, static_cast<std::size_t>(pos + 20));
        const quint32 uncompSize = readU32(bytes, static_cast<std::size_t>(pos + 24));
        const quint16 nameLen = readU16(bytes, static_cast<std::size_t>(pos + 28));
        const quint16 extraLen = readU16(bytes, static_cast<std::size_t>(pos + 30));
        const quint16 commentLen = readU16(bytes, static_cast<std::size_t>(pos + 32));
        const quint32 localOff = readU32(bytes, static_cast<std::size_t>(pos + 42));
        const qint64 entryEnd = qint64(pos) + 46 + nameLen + extraLen + commentLen;
        if (entryEnd > m_bytes.size())
            return;
        const QString name = QString::fromUtf8(m_bytes.constData() + pos + 46, nameLen);

        m_entries.insert(name, Entry{
            .localHeaderOffset = localOff,
            .compressedSize = compSize,
            .uncompressedSize = uncompSize,
            .method = method,
        });

        pos = static_cast<int>(entryEnd);
    }
    m_valid = true;
}

QStringList ZipReader::fileNames() const
{
    return m_entries.keys();
}

QByteArray ZipReader::fileData(const QString &fileName) const
{
    if (!m_valid || !m_entries.contains(fileName))
        return {};

    const Entry e = m_entries.value(fileName);
    const quint32 localOff = e.localHeaderOffset;
    const auto bytes = asBytes(m_bytes);
    if (qint64(localOff) + 30 > m_bytes.size())
        return {};
    const int local = static_cast<int>(localOff);
    if (readU32(bytes, static_cast<std::size_t>(local)) != 0x04034b50)
        return {};

    const quint16 nameLen = readU16(bytes, static_cast<std::size_t>(local + 26));
    const quint16 extraLen = readU16(bytes, static_cast<std::size_t>(local + 28));
    const qint64 dataOff = qint64(local) + 30 + nameLen + extraLen;
    if (dataOff + qint64(e.compressedSize) > m_bytes.size()
        || e.compressedSize > static_cast<quint32>(INT_MAX))
        return {};

    const QByteArray compressed =
        m_bytes.mid(static_cast<int>(dataOff), static_cast<int>(e.compressedSize));
    if (e.method == 0)
        return compressed;
    if (e.method == 8)
        return inflateRaw(compressed, e.uncompressedSize);
    return {};
}

ZipWriter::ZipWriter(const QString &filePath)
    : m_filePath(filePath)
{
    m_valid = !filePath.isEmpty();
}

ZipWriter::~ZipWriter()
{
    if (m_valid && !m_closed)
        (void)close();
}

void ZipWriter::addFile(const QString &fileName, const QByteArray &data)
{
    if (!m_valid || m_closed)
        return;

    Written w{
        .name = fileName,
        .crc = crc32Of(data),
        .uncompressedSize = quint32(data.size()),
    };

    const QByteArray deflated = deflateRaw(data);
    if (!deflated.isEmpty() && deflated.size() < data.size()) {
        w.method = 8;
        w.compressed = deflated;
    } else {
        w.method = 0;
        w.compressed = data;
    }
    w.compressedSize = quint32(w.compressed.size());
    m_files.append(std::move(w));
}

bool ZipWriter::close()
{
    if (!m_valid || m_closed)
        return m_valid;
    m_closed = true;

    QByteArray out;
    out.reserve(1024 * 64);

    for (Written &w : m_files) {
        w.localHeaderOffset = quint32(out.size());
        appendU32(&out, 0x04034b50);
        appendU16(&out, 20); // version needed
        appendU16(&out, 0);  // flags
        appendU16(&out, w.method);
        appendU16(&out, 0); // time
        appendU16(&out, 0); // date
        appendU32(&out, w.crc);
        appendU32(&out, w.compressedSize);
        appendU32(&out, w.uncompressedSize);
        const QByteArray name = w.name.toUtf8();
        appendU16(&out, quint16(name.size()));
        appendU16(&out, 0); // extra
        out.append(name);
        out.append(w.compressed);
    }

    const quint32 cdOffset = quint32(out.size());
    for (const Written &w : m_files) {
        appendU32(&out, 0x02014b50);
        appendU16(&out, 20); // version made by
        appendU16(&out, 20); // version needed
        appendU16(&out, 0);
        appendU16(&out, w.method);
        appendU16(&out, 0);
        appendU16(&out, 0);
        appendU32(&out, w.crc);
        appendU32(&out, w.compressedSize);
        appendU32(&out, w.uncompressedSize);
        const QByteArray name = w.name.toUtf8();
        appendU16(&out, quint16(name.size()));
        appendU16(&out, 0); // extra
        appendU16(&out, 0); // comment
        appendU16(&out, 0); // disk
        appendU16(&out, 0); // int attr
        appendU32(&out, 0); // ext attr
        appendU32(&out, w.localHeaderOffset);
        out.append(name);
    }
    const quint32 cdSize = quint32(out.size()) - cdOffset;

    appendU32(&out, 0x06054b50);
    appendU16(&out, 0);
    appendU16(&out, 0);
    appendU16(&out, quint16(m_files.size()));
    appendU16(&out, quint16(m_files.size()));
    appendU32(&out, cdSize);
    appendU32(&out, cdOffset);
    appendU16(&out, 0); // comment

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_valid = false;
        return false;
    }
    if (file.write(out) != out.size()) {
        m_valid = false;
        return false;
    }
    return true;
}
