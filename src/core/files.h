// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QStringList>
#include <QUrl>

// A shared URL that cannot be sent; the callers turn it into a translated message
struct UrlProblem {
    enum Reason {
        NotLocal,
        Missing,
        Unreadable,
        Unsupported, // neither a regular file nor a folder, e.g. a socket or a device
    };
    Reason reason;
    QUrl url;
};

struct ValidatedUrls {
    QStringList files; // absolute paths of readable regular files, in the order given
    QStringList folders; // absolute paths of readable folders, which have to be archived before sending
    qint64 totalBytes = 0; // size of files; folders are not counted
    QList<UrlProblem> problems;
};

// Sorts the shared URLs, as Purpose passes them, into files, folders and problems. A path given more than once is
// kept only the first time.
ValidatedUrls validateUrls(const QStringList &urls);
