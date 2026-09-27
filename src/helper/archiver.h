// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sendqueue.h"

#include <KLocalizedString>
#include <QFile>
#include <QStringList>

#include <atomic>
#include <memory>

#include <sys/stat.h>

// Packs a folder into a new archive in QStandardPaths::CacheLocation, on a worker thread.
// The archive holds the folder itself (so it unpacks into one folder), with its hidden files and empty folders.
// Symbolic links are stored as links, never followed; other special files (sockets, pipes, devices) are left out.
class Archiver : public Packer
{
    Q_OBJECT
public:
    explicit Archiver(QObject *parent = nullptr);
    ~Archiver() override;

    // The formats this KArchive build can write, in the order they are offered; tar.zst needs zstd support
    static QStringList formats();
    // Removes the archives an earlier run left in the cache when it was killed while sending them
    static void removeLeftovers();

    void pack(const QString &folder, const QString &format) override;
    // Returns at once: the worker thread may hang on a folder that does not reply, and removes its archive when it stops
    void cancel() override;

private:
    // tells the worker thread of the run in progress to stop; shared with it, since it may outlive the Archiver
    std::shared_ptr<std::atomic_bool> m_cancelled;
};

// Opens file for reading only if it is a regular file under root, a canonical path, so that neither it nor a folder
// on the way can have been swapped for a link after it was listed. info is what was opened. Empty when opened.
// Only file contents are guarded: a folder swapped for a link still adds the names and link targets under it.
KLocalizedString openInside(const QString &root, QFile &file, struct stat &info);
