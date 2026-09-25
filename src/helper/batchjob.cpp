// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "batchjob.h"

#include <KLocalizedString>

#include <algorithm>

BatchJob::BatchJob(SendQueue *queue, int batchId, QObject *parent)
    : KJob(parent)
    , m_queue(queue)
    , m_batchId(batchId)
{
    setCapabilities(KJob::Killable);
    // Notifier tells how it went
    setFinishedNotificationHidden();
}

void BatchJob::start()
{
    connect(m_queue, &SendQueue::batchChanged, this, [this](int batchId) {
        if (batchId == m_batchId) {
            update();
        }
    });
    connect(m_queue, &SendQueue::batchFinished, this, [this](int batchId) {
        if (batchId != m_batchId) {
            // one batch less ahead of this one
            update();
        } else if (!m_killing) {
            // kill() emits the result itself
            update();
            emitResult();
        }
    });
    Q_EMIT description(this, title());
    m_speedClock.start();
    update();
}

bool BatchJob::doKill()
{
    m_killing = true;
    m_queue->cancel(m_batchId);
    return true;
}

QString BatchJob::title() const
{
    const SendBatch *batch = m_queue->batch(m_batchId);
    return i18n("Sending to %1", batch ? batch->deviceName : QString());
}

void BatchJob::update()
{
    const SendBatch *batch = m_queue->batch(m_batchId);
    if (!batch) {
        return;
    }
    const qint64 sent = batch->sentBytes();
    setTotalAmount(KJob::Files, batch->items.size());
    setProcessedAmount(KJob::Files, batch->doneCount());
    setTotalAmount(KJob::Bytes, batch->totalBytes());
    setProcessedAmount(KJob::Bytes, sent);

    if (sent != m_progressBytes) {
        m_progressBytes = sent;
        if (m_speedClock.elapsed() >= 1000) {
            emitSpeed((sent - m_speedBytes) * 1000 / m_speedClock.restart());
            m_speedBytes = sent;
        }
    }

    const auto active = std::find_if(batch->items.cbegin(), batch->items.cend(), [](const SendItem &item) {
        return item.state == SendItem::Sending || item.state == SendItem::Finishing;
    });
    QString message;
    if (active == batch->items.cend()) {
        // the next item is new, even when it has the name of the one that just failed
        m_file.clear();
        const int ahead = m_queue->batchesAhead(m_batchId);
        if (ahead > 0 && !batch->isFinished()) {
            message = i18ncp("@info:status", "Queued, %1 send ahead", "Queued, %1 sends ahead", ahead);
        }
    } else {
        if (active->fileName != m_file) {
            m_file = active->fileName;
            Q_EMIT description(this, title(), {i18nc("The file being sent", "File"), m_file});
        }
        if (active->state != m_state) {
            // time spent queued is no upload; from one file to the next it is
            if (active->state == SendItem::Sending && m_state != SendItem::Finishing) {
                m_speedClock.restart();
                m_speedBytes = sent;
            }
            m_state = active->state;
        }
        if (active->state == SendItem::Finishing) {
            message = i18nc("@info:status waiting for the device to confirm the file", "Finishing…");
        }
    }
    if (message != m_message) {
        m_message = message;
        Q_EMIT infoMessage(this, message);
    }
}
