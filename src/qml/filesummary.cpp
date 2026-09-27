// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "filesummary.h"
#include "archiver.h"
#include "settings.h"
#include "urlproblemtext.h"

#include <KFormat>
#include <QSettings>

using namespace Qt::StringLiterals;

QVariantMap FileSummary::summarize(const QStringList &urls) const
{
    const ValidatedUrls validated = validateUrls(urls);
    const qsizetype files = validated.files.size();
    const qsizetype folders = validated.folders.size();

    const QString filesText =
        i18ncp("@info the shared files and their size", "%1 file (%2)", "%1 files (%2)", files, KFormat().formatByteSize(validated.totalBytes));
    const QString foldersText = i18ncp("@info the shared folders", "%1 folder", "%1 folders", folders);
    QString shared = files != 0 ? filesText : foldersText;
    if (files != 0 && folders != 0) {
        shared = i18nc("@info shared files and folders", "%1 and %2", filesText, foldersText);
    }
    const QString text = files + folders != 0 ? i18nc("@title %1 is what is shared", "Send %1 to:", shared) : i18nc("@title", "Choose a device to send to:");

    return {
        {u"text"_s, text},
        {u"hasFolders"_s, folders != 0},
        {u"problem"_s, validated.problems.isEmpty() ? QString() : urlProblemText(validated.problems.first())},
    };
}

QStringList FileSummary::archiveFormats() const
{
    return Archiver::formats();
}

QString FileSummary::archiveFormat() const
{
    const QString format = QSettings(Settings::Name).value(Settings::ArchiveFormatKey).toString();
    const QStringList formats = archiveFormats();
    return formats.contains(format) ? format : formats.value(0);
}

void FileSummary::setArchiveFormat(const QString &format)
{
    if (format != archiveFormat()) {
        QSettings(Settings::Name).setValue(Settings::ArchiveFormatKey, format);
        Q_EMIT archiveFormatChanged();
    }
}
