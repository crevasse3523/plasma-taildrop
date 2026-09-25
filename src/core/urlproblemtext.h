// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "files.h"

#include <KLocalizedString>

// Header only, so that the library itself does not need KI18n: the Share dialog and the plugin job both link it
inline QString urlProblemText(const UrlProblem &problem)
{
    const QString name = problem.url.toDisplayString(QUrl::PreferLocalFile);
    switch (problem.reason) {
    case UrlProblem::NotLocal:
        return i18n("Only local files can be sent: %1", name);
    case UrlProblem::Missing:
        return i18n("%1 does not exist", name);
    case UrlProblem::Unreadable:
        return i18n("Could not read %1", name);
    case UrlProblem::Unsupported:
        return i18n("%1 is neither a file nor a folder", name);
    }
    return name;
}
