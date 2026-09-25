// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "files.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

ValidatedUrls validateUrls(const QStringList &urls)
{
    ValidatedUrls result;
    QSet<QString> seen;
    for (const QString &text : urls) {
        const QUrl url(text);
        if (!url.isLocalFile()) {
            result.problems.append({UrlProblem::NotLocal, url});
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        const QString path = QDir::cleanPath(info.absoluteFilePath());
        if (seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        if (!info.exists()) {
            result.problems.append({UrlProblem::Missing, url});
        } else if (!info.isReadable()) {
            result.problems.append({UrlProblem::Unreadable, url});
        } else if (info.isDir()) {
            result.folders.append(path);
        } else if (info.isFile()) {
            result.files.append(path);
            result.totalBytes += info.size();
        } else {
            result.problems.append({UrlProblem::Unsupported, url});
        }
    }
    return result;
}
