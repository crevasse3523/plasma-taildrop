// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sendqueue.h"

#include <KJob>
#include <QElapsedTimer>
#include <QTimer>

// Shows one batch of the queue in the Plasma job view, with its progress and a Cancel button.
// Ends when the batch does; how it went is told by Notifier, so the job itself never fails.
class BatchJob : public KJob
{
    Q_OBJECT
public:
    // Upload without progress for this long is shown as a device that does not reply
    static constexpr int QuietMs = 15000;

    // quietMs replaces QuietMs, for the tests
    BatchJob(SendQueue *queue, int batchId, QObject *parent = nullptr, int quietMs = QuietMs);

    void start() override;

protected:
    bool doKill() override;

private:
    QString title() const;
    void update();
    void restartQuiet();

    SendQueue *m_queue;
    int m_batchId;
    QString m_file; // the one shown in the description
    SendItem::State m_state = SendItem::Queued; // of the item being worked on
    QString m_message; // the info message shown
    int m_packedPercent = 0;
    bool m_killing = false;
    // no progress for a while
    QTimer m_quiet;
    bool m_isQuiet = false;
    qint64 m_progressBytes = 0; // sent at the last update
    // bytes and time of the last speed update
    QElapsedTimer m_speedClock;
    qint64 m_speedBytes = 0;
};
