// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "archiver.h"

#include <KCompressionDevice>
#include <KFilterBase>
#include <KLocalizedString>
#include <KTar>
#include <KZip>
#include <QDir>
#include <QDirIterator>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QtConcurrentRun>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <functional>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace
{
struct Format {
    QLatin1StringView extension;
    KCompressionDevice::CompressionType compression; // None for zip
};

constexpr Format Formats[] = {
    {"zip"_L1, KCompressionDevice::None},
    {"tar.gz"_L1, KCompressionDevice::GZip},
    {"tar.xz"_L1, KCompressionDevice::Xz},
    {"tar.zst"_L1, KCompressionDevice::Zstd},
};

// Starts the name of every archive in the cache, so that removeLeftovers() finds them
constexpr auto ArchivePrefix = "plasma-taildrop-"_L1;

// KZip writes no zip64, so the sizes and offsets in the whole archive must stay below 0xFFFFFFFF, which marks zip64
constexpr qint64 ZipLimit = 0xFFFFFFFF;

// KZip writes no zip64 either for the count of entries, which zip keeps in 16 bits and where 0xFFFF marks zip64
constexpr qint64 ZipEntryLimit = 0xFFFF;

// What a zip needs for every entry besides its data and the name, which the local and the central header both hold
constexpr qint64 ZipEntryOverhead = 128;

// Files are read in pieces this big, which is also how often progress is reported and cancelling is checked
constexpr qint64 ChunkSize = 1024 * 1024;

bool isSupported(const Format &format)
{
    // KArchive can be built without some of the compression libraries
    return format.compression == KCompressionDevice::None
        || std::unique_ptr<KFilterBase>(KCompressionDevice::filterForCompressionType(format.compression)) != nullptr;
}

// Type and permission bits as stored in the archive; 0 when the entry is gone
mode_t fileMode(const QString &path)
{
    struct stat info;
    return ::lstat(QFile::encodeName(path).constData(), &info) == 0 ? info.st_mode : 0;
}

// reason is a QString, or a KLocalizedString that is translated on the GUI thread with the rest
template<typename Reason>
KLocalizedString readError(const QString &path, const Reason &reason)
{
    return ki18n("Could not read %1: %2").subs(path).subs(reason);
}

// Links are not followed, so only the files really in the folder count
qint64 dataSize(const QFileInfo &info)
{
    return info.isFile() && !info.isSymLink() ? info.size() : 0;
}

qint64 zipEntrySize(const QFileInfo &info)
{
    // the full path stands in for the name in the archive, which is usually shorter
    return dataSize(info) + ZipEntryOverhead + 2 * qint64(info.filePath().toUtf8().size());
}

// QDirIterator stops at the first name that is not valid UTF-8, which would leave the rest of the folder out silently
bool hasUndecodableName(const QString &folder)
{
    DIR *dir = ::opendir(QFile::encodeName(folder).constData());
    if (!dir) {
        // packing reports a folder it cannot read
        return false;
    }
    bool found = false;
    for (const dirent *entry; !found && (entry = ::readdir(dir));) {
        const QByteArray name(entry->d_name);
        found = QFile::encodeName(QFile::decodeName(name)) != name;
    }
    ::closedir(dir);
    return found;
}

struct Result {
    QString archivePath;
    // translated on the GUI thread: KLocalizedString sets itself up on first use, which only works there
    KLocalizedString error;
};

// Called with the bytes packed so far and the bytes of all files
using Progress = std::function<void(qint64, qint64)>;

Result packFolder(const QString &sharedFolder, const QString &extension, const std::atomic_bool &cancelled, const Progress &progress)
{
    const auto format = std::find_if(std::begin(Formats), std::end(Formats), [&extension](const Format &format) {
        return format.extension == extension;
    });
    if (format == std::end(Formats) || !isSupported(*format)) {
        return {QString(), ki18n("Archives of type %1 are not supported.").subs(extension)};
    }

    // the shared folder may be a link itself; what is in it is packed under the name it was shared with
    const QString rootName = QFileInfo(sharedFolder).fileName();
    const QString folder = QFileInfo(sharedFolder).canonicalFilePath();
    if (folder.isEmpty()) {
        return {QString(), readError(sharedFolder, ki18n("it does not exist"))};
    }
    const bool isZip = format->compression == KCompressionDevice::None;

    // made before the folder is counted, so that both passes can leave it out when the folder holds the cache
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QTemporaryFile file(cache + u'/' + ArchivePrefix + u"XXXXXX."_s + format->extension);
    file.setAutoRemove(false);
    if (!QDir().mkpath(cache) || !file.open()) {
        return {QString(), ki18n("Could not create an archive in %1: %2").subs(cache).subs(file.errorString())};
    }
    const QString archivePath = file.fileName();
    file.close();
    // this archive and the others still waiting to be sent
    const QString cacheFolder = QFileInfo(cache).canonicalFilePath();
    const QString cacheContents = cacheFolder + u'/';
    const auto isCache = [&cacheFolder, &cacheContents](const QFileInfo &info) {
        return info.filePath() == cacheFolder || info.filePath().startsWith(cacheContents);
    };

    std::unique_ptr<KArchive> archive;
    if (isZip) {
        auto zip = std::make_unique<KZip>(archivePath);
        // deflate cannot shrink photos and videos, and packs them slower than a LAN sends them
        zip->setCompression(KZip::NoCompression);
        archive = std::move(zip);
    } else {
        // KTar picks the compression from the extension
        archive = std::make_unique<KTar>(archivePath);
    }
    const auto fail = [&archive, &archivePath](const KLocalizedString &error) {
        archive.reset();
        QFile::remove(archivePath);
        return Result{QString(), error};
    };
    const auto archiveError = [&archive, &sharedFolder] {
        return ki18n("Could not pack %1: %2").subs(sharedFolder).subs(archive->errorString());
    };
    const KLocalizedString zipTooLarge = ki18n("zip cannot hold more than 4 GiB, choose a tar format.");

    constexpr auto entries = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;
    qint64 total = 0;
    qint64 zipSize = 0;
    qint64 zipEntries = 0;
    const auto count = [&](const QFileInfo &info) -> KLocalizedString {
        total += dataSize(info);
        zipSize += zipEntrySize(info);
        // other special files are left out
        zipEntries += info.isDir() || info.isFile() || info.isSymLink();
        const bool isFolder = info.isDir() && !info.isSymLink();
        return isFolder && hasUndecodableName(info.filePath()) ? readError(info.filePath(), ki18n("a name is not valid UTF-8")) : KLocalizedString();
    };
    // both passes must visit the same entries, or the count and the zip limits no longer match what is packed
    const auto walk = [&](const auto &visit) {
        KLocalizedString error = visit(QFileInfo(folder));
        for (QDirIterator it(folder, entries, QDirIterator::Subdirectories); error.isEmpty() && !cancelled && it.hasNext();) {
            const QFileInfo info = it.nextFileInfo();
            if (!isCache(info)) {
                error = visit(info);
            }
        }
        return error;
    };
    KLocalizedString error = walk(count);
    if (error.isEmpty() && isZip && zipSize >= ZipLimit) {
        error = zipTooLarge;
    }
    if (error.isEmpty() && isZip && zipEntries >= ZipEntryLimit) {
        error = ki18n("zip cannot hold this many files and folders, choose a tar format.");
    }
    if (!error.isEmpty() || cancelled) {
        return fail(error);
    }

    if (!archive->open(QIODevice::WriteOnly)) {
        return fail(archiveError());
    }
    const auto check = [&archiveError](bool written) {
        return written ? KLocalizedString() : archiveError();
    };

    const QDir root(folder);
    qint64 packed = 0;
    // counted again, since the folder may have grown meanwhile
    zipSize = 0;
    const auto add = [&](const QFileInfo &info) -> KLocalizedString {
        zipSize += zipEntrySize(info);
        if (isZip && zipSize >= ZipLimit) {
            return zipTooLarge;
        }
        const QString relativePath = root.relativeFilePath(info.filePath());
        const QString name = relativePath == u"."_s ? rootName : rootName + u'/' + relativePath;
        const mode_t mode = fileMode(info.filePath());
        const QDateTime accessed = info.lastRead();
        const QDateTime modified = info.lastModified();
        const QDateTime changed = info.metadataChangeTime();
        if (S_ISLNK(mode)) {
            // the link as it is, e.g. "../photo.jpg"; what it points to may not even be in the folder
            return check(archive->writeSymLink(name, info.readSymLink(), info.owner(), info.group(), mode, accessed, modified, changed));
        }
        if (S_ISDIR(mode)) {
            // QDirIterator would silently skip what is in a folder it cannot list
            if (!info.isReadable() || !info.isExecutable()) {
                return readError(info.filePath(), qt_error_string(EACCES));
            }
            return check(archive->writeDir(name, info.owner(), info.group(), mode, accessed, modified, changed));
        }
        if (!S_ISREG(mode)) {
            return KLocalizedString();
        }
        QFile source(info.filePath());
        struct stat opened;
        if (const KLocalizedString error = openInside(folder, source, opened); !error.isEmpty()) {
            return error;
        }
        // tar needs the size up front, so a file that changes meanwhile is read only up to its size back then
        const qint64 size = opened.st_size;
        if (!archive->prepareWriting(name, info.owner(), info.group(), size, opened.st_mode, accessed, modified, changed)) {
            return archiveError();
        }
        for (qint64 left = size; left > 0;) {
            if (cancelled) {
                return KLocalizedString();
            }
            const QByteArray chunk = source.read(std::min(left, ChunkSize));
            if (chunk.isEmpty()) {
                // no error means the end of the file came before its size back then
                return source.error() == QFileDevice::NoError ? readError(info.filePath(), ki18n("it got shorter while it was being packed"))
                                                              : readError(info.filePath(), source.errorString());
            }
            if (!archive->writeData(chunk)) {
                return archiveError();
            }
            left -= chunk.size();
            packed += chunk.size();
            // the folder may have grown since it was counted
            progress(std::min(packed, total), total);
        }
        return check(archive->finishWriting(size));
    };

    error = walk(add);
    if (error.isEmpty() && !cancelled && !archive->close()) {
        error = archiveError();
    }
    if (!error.isEmpty() || cancelled) {
        return fail(error);
    }
    return {archivePath, KLocalizedString()};
}
}

KLocalizedString openInside(const QString &root, QFile &file, struct stat &info)
{
    // O_NONBLOCK, or a pipe swapped in would wait for a writer
    const int fd = ::open(QFile::encodeName(file.fileName()).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return readError(file.fileName(), qt_error_string(errno));
    }
    // where it really is, whatever the folders on the way have become
    char target[PATH_MAX];
    const ssize_t length = ::readlink(QByteArray("/proc/self/fd/" + QByteArray::number(fd)).constData(), target, sizeof target);
    const bool inside = length > 0 && QByteArrayView(target, length).startsWith(QByteArray(QFile::encodeName(root) + '/'));
    if (!inside || ::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        ::close(fd);
        return readError(file.fileName(), ki18n("it was replaced while it was being packed"));
    }
    // QFile takes over the descriptor only when it opens it
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return readError(file.fileName(), file.errorString());
    }
    return KLocalizedString();
}

Archiver::Archiver(QObject *parent)
    : Packer(parent)
{
}

Archiver::~Archiver()
{
    cancel();
}

QStringList Archiver::formats()
{
    QStringList formats;
    for (const Format &format : Formats) {
        if (isSupported(format)) {
            formats.append(format.extension);
        }
    }
    return formats;
}

void Archiver::removeLeftovers()
{
    QDir cache(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    for (const QString &name : cache.entryList({ArchivePrefix + u'*'}, QDir::Files)) {
        cache.remove(name);
    }
}

void Archiver::pack(const QString &folder, const QString &format)
{
    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    m_cancelled = cancelled;
    // not a child: after the Archiver is gone it still removes the archive of a run cancelled when it was done
    auto *watcher = new QFutureWatcher<Result>;
    // the Archiver is alive while its run is not cancelled, as the destructor cancels it
    const auto progress = [this, watcher, cancelled](qint64 packed, qint64 total) {
        QMetaObject::invokeMethod(
            watcher,
            [this, cancelled, packed, total] {
                if (!*cancelled) {
                    Q_EMIT Packer::progress(packed, total);
                }
            },
            Qt::QueuedConnection);
    };
    connect(watcher, &QFutureWatcherBase::finished, watcher, [this, watcher, cancelled] {
        const Result result = watcher->result();
        watcher->deleteLater();
        if (!*cancelled) {
            m_cancelled.reset();
            Q_EMIT finished(result.archivePath, result.error.isEmpty() ? QString() : result.error.toString());
        } else if (!result.archivePath.isEmpty()) {
            // it was done just before it was cancelled
            QFile::remove(result.archivePath);
        }
    });
    watcher->setFuture(QtConcurrent::run([folder, format, cancelled, progress] {
        return packFolder(folder, format, *cancelled, progress);
    }));
}

void Archiver::cancel()
{
    if (m_cancelled) {
        *m_cancelled = true;
        m_cancelled.reset();
    }
}
