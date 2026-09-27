// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "notifier.h"
#include "sendqueue.h"

#include <KLocalizedString>
#include <KNotification>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
QString reason(const SendItem &item)
{
    switch (item.failure) {
    case SendItem::NotOperator:
        return i18n("you are not allowed to send files; run %1", LocalApi::OperatorCommand);
    case SendItem::NodeNotFound:
        return i18n("the device is gone or does not accept files");
    case SendItem::DaemonDown:
        return i18n("Tailscale is not running");
    case SendItem::PeerUnreachable:
        return i18n("the device could not be reached");
    case SendItem::Stalled:
        return i18n("the device stopped replying");
    case SendItem::FileError:
    case SendItem::Other:
    case SendItem::NotSent:
    case SendItem::NoFailure:
        break;
    }
    return item.errorString.isEmpty() ? i18n("failed") : item.errorString;
}
}

QString failureLine(const SendItem &item)
{
    return i18nc("@info file name: why it failed", "%1: %2", item.fileName.toHtmlEscaped(), reason(item).toHtmlEscaped());
}

void Notifier::notify(const SendBatch &batch)
{
    const int batchId = batch.id;
    const qsizetype done = batch.doneCount();
    const bool cancelled = std::any_of(batch.items.cbegin(), batch.items.cend(), [](const SendItem &item) {
        return item.state == SendItem::Cancelled;
    });
    const bool succeeded = done == batch.items.size();

    auto notification = new KNotification(succeeded ? u"transferFinished"_s : u"transferFailed"_s);
    notification->setComponentName(u"plasma-taildrop"_s);
    if (succeeded) {
        notification->setTitle(i18n("Sent to %1", batch.deviceName));
        notification->setText(batch.items.size() == 1 ? batch.items.first().fileName.toHtmlEscaped()
                                                      : i18np("%1 file sent", "%1 files sent", batch.items.size()));
    } else {
        notification->setTitle(cancelled ? i18n("Sending to %1 cancelled", batch.deviceName) : i18n("Sending to %1 failed", batch.deviceName));
        QStringList lines{i18ncp("@info %2 is how many of the files were sent", "%2 of %1 file sent", "%2 of %1 files sent", batch.items.size(), done)};
        // the files that were not tried fail for the same reason as the one before them
        for (const SendItem &item : batch.items) {
            if (item.state == SendItem::Failed && item.failure != SendItem::NotSent) {
                lines.append(failureLine(item));
            }
        }
        notification->setText(lines.join(u'\n'));
        connect(notification->addAction(i18n("Retry")), &KNotificationAction::activated, this, [this, batchId] {
            Q_EMIT retryRequested(batchId);
        });
    }
    m_shown.insert(batchId, notification);
    connect(notification, &KNotification::closed, this, [this, notification, batchId] {
        // a retry that failed at once shows its notification before the old one is closed
        if (m_shown.value(batchId) == notification) {
            m_shown.remove(batchId);
            Q_EMIT closed(batchId);
        }
    });
    notification->sendEvent();
}
