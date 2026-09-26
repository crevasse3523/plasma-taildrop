// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QObject>

class KNotification;
struct SendBatch;
struct SendItem;

// The line of a failed item in the notification text, which the notification server renders as markup
QString failureLine(const SendItem &item);

// Tells how a batch ended, with the events of plasma-taildrop.notifyrc. A batch that did not fully succeed
// gets a Retry action, which works while this process runs.
class Notifier : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    void notify(const SendBatch &batch);

Q_SIGNALS:
    void retryRequested(int batchId);
    // the newest notification of the batch was closed; an older one that a retry replaced says nothing
    void closed(int batchId);

private:
    QHash<int, KNotification *> m_shown;
};
