// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sendqueue.h"

#include <QFileInfo>

#include <algorithm>

bool SendBatch::isFinished() const
{
    return std::all_of(items.cbegin(), items.cend(), [](const SendItem &item) {
        return item.state >= SendItem::Done;
    });
}

qint64 SendBatch::totalBytes() const
{
    qint64 total = 0;
    for (const SendItem &item : items) {
        total += item.size;
    }
    return total;
}

qint64 SendBatch::sentBytes() const
{
    qint64 sent = 0;
    for (const SendItem &item : items) {
        sent += item.sent;
    }
    return sent;
}

qsizetype SendBatch::doneCount() const
{
    return std::count_if(items.cbegin(), items.cend(), [](const SendItem &item) {
        return item.state == SendItem::Done;
    });
}

SendQueue::SendQueue(Transport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    connect(m_transport, &Transport::progress, this, &SendQueue::onProgress);
    connect(m_transport, &Transport::finished, this, &SendQueue::onSent);
}

SendQueue::~SendQueue()
{
    if (m_activeIndex < 0) {
        return;
    }
    // no signals from here on: whoever listens may already be gone
    disconnect(m_transport, nullptr, this, nullptr);
    stopActive();
}

int SendQueue::enqueue(const QString &stableId, const QString &deviceName, const QStringList &files)
{
    SendBatch batch;
    batch.id = m_nextId++;
    batch.stableId = stableId;
    batch.deviceName = deviceName;
    for (const QString &file : files) {
        SendItem item;
        item.path = file;
        item.fileName = QFileInfo(file).fileName();
        item.size = QFileInfo(file).size();
        batch.items.append(item);
    }
    m_batches.append(batch);
    startNext();
    return batch.id;
}

void SendQueue::cancel(int batchId)
{
    const SendBatch *batch = find(batchId);
    if (!batch || batch->isFinished()) {
        return;
    }
    if (m_activeIndex >= 0 && m_activeBatch == batchId) {
        stopActive();
        endActive(SendItem::Cancelled, SendItem::NoFailure, {});
    }
    endRest(batchId, SendItem::Cancelled, SendItem::NoFailure);
    Q_EMIT batchFinished(batchId);
    startNext();
}

void SendQueue::retry(int batchId)
{
    SendBatch *batch = find(batchId);
    if (!batch || !batch->isFinished()) {
        return;
    }
    bool retried = false;
    for (SendItem &item : batch->items) {
        if (item.state == SendItem::Failed || item.state == SendItem::Cancelled) {
            // tailscaled continues where the device's partial file ends, so starting over costs little
            item.state = SendItem::Queued;
            item.failure = SendItem::NoFailure;
            item.errorString.clear();
            item.sent = 0;
            retried = true;
        }
    }
    if (!retried) {
        return;
    }
    m_batches.move(batch - m_batches.data(), m_batches.size() - 1);
    Q_EMIT batchChanged(batchId);
    startNext();
}

void SendQueue::forget(int batchId)
{
    m_batches.removeIf([batchId](const SendBatch &batch) {
        return batch.id == batchId && batch.isFinished();
    });
}

const SendBatch *SendQueue::batch(int batchId) const
{
    for (const SendBatch &batch : m_batches) {
        if (batch.id == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

int SendQueue::batchesAhead(int batchId) const
{
    int ahead = 0;
    for (const SendBatch &batch : m_batches) {
        if (batch.id == batchId) {
            break;
        }
        ahead += batch.isFinished() ? 0 : 1;
    }
    return ahead;
}

bool SendQueue::isBusy() const
{
    return std::any_of(m_batches.cbegin(), m_batches.cend(), [](const SendBatch &batch) {
        return !batch.isFinished();
    });
}

SendBatch *SendQueue::find(int batchId)
{
    return const_cast<SendBatch *>(batch(batchId));
}

SendItem &SendQueue::activeItem()
{
    return find(m_activeBatch)->items[m_activeIndex];
}

void SendQueue::startNext()
{
    while (m_activeIndex < 0) {
        for (const SendBatch &batch : std::as_const(m_batches)) {
            const auto item = std::find_if(batch.items.cbegin(), batch.items.cend(), [](const SendItem &item) {
                return item.state == SendItem::Queued;
            });
            if (item != batch.items.cend()) {
                m_activeBatch = batch.id;
                m_activeIndex = item - batch.items.cbegin();
                break;
            }
        }
        if (m_activeIndex < 0) {
            return;
        }
        // ends the item at once when the file cannot be read; then the loop goes on with the next one
        upload();
    }
}

void SendQueue::upload()
{
    const SendBatch *batch = find(m_activeBatch);
    SendItem &item = activeItem();
    QString errorString;
    if (!m_transport->send(batch->stableId, item.path, item.fileName, &errorString)) {
        const int batchId = m_activeBatch;
        endActive(SendItem::Failed, SendItem::FileError, errorString);
        finishIfDone(batchId);
        return;
    }
    item.state = SendItem::Sending;
    Q_EMIT batchChanged(m_activeBatch);
}

void SendQueue::stopActive()
{
    m_transport->abort();
}

void SendQueue::endActive(SendItem::State state, SendItem::Failure failure, const QString &errorString)
{
    SendItem &item = activeItem();
    item.state = state;
    item.failure = failure;
    item.errorString = errorString;
    if (state == SendItem::Done) {
        item.sent = item.size;
    }
    m_activeIndex = -1;
    Q_EMIT batchChanged(m_activeBatch);
}

void SendQueue::endRest(int batchId, SendItem::State state, SendItem::Failure failure)
{
    bool ended = false;
    for (SendItem &item : find(batchId)->items) {
        if (item.state == SendItem::Queued) {
            item.state = state;
            item.failure = failure;
            ended = true;
        }
    }
    if (ended) {
        Q_EMIT batchChanged(batchId);
    }
}

void SendQueue::finishIfDone(int batchId)
{
    if (find(batchId)->isFinished()) {
        Q_EMIT batchFinished(batchId);
    }
}

void SendQueue::onProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (m_activeIndex < 0 || bytesSent == activeItem().sent) {
        return;
    }
    SendItem &item = activeItem();
    item.sent = bytesSent;
    // the file may have changed since it was queued
    item.size = bytesTotal;
    if (bytesSent == bytesTotal) {
        item.state = SendItem::Finishing;
    }
    Q_EMIT batchChanged(m_activeBatch);
}

void SendQueue::onSent(LocalApi::Outcome outcome, const QString &message)
{
    if (m_activeIndex < 0) {
        return;
    }
    const int batchId = m_activeBatch;
    // the next files would meet the same refusal or the same unreachable device
    bool stopsBatch = true;
    SendItem::Failure failure = SendItem::Other;
    switch (outcome) {
    case LocalApi::Outcome::Ok:
        failure = SendItem::NoFailure;
        break;
    case LocalApi::Outcome::NotOperator:
        failure = SendItem::NotOperator;
        break;
    case LocalApi::Outcome::DeviceRefused:
        // Other keeps the device's own reason as the message, without the operator hint
        break;
    case LocalApi::Outcome::NodeNotFound:
        failure = SendItem::NodeNotFound;
        break;
    case LocalApi::Outcome::DaemonDown:
        failure = SendItem::DaemonDown;
        break;
    case LocalApi::Outcome::PeerUnreachable:
        failure = SendItem::PeerUnreachable;
        break;
    case LocalApi::Outcome::Cancelled:
    case LocalApi::Outcome::Other:
        // something about this file, e.g. a name the device does not take; the others may still work
        stopsBatch = false;
        break;
    }
    if (failure == SendItem::NoFailure) {
        endActive(SendItem::Done, SendItem::NoFailure, {});
    } else {
        endActive(SendItem::Failed, failure, message);
        if (stopsBatch) {
            endRest(batchId, SendItem::Failed, SendItem::NotSent);
        }
    }
    finishIfDone(batchId);
    startNext();
}
