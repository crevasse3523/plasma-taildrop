// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "localapi.h"

#include <QList>
#include <QObject>
#include <QString>

class QTimer;

// Uploads one file to a device at a time. After abort() it reports nothing more about that upload.
class Transport : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // Starts sending filePath as fileName; returns false with errorString set when the file cannot be read.
    // Once started, finished() always comes later, never from within send().
    virtual bool send(const QString &stableId, const QString &filePath, const QString &fileName, QString *errorString) = 0;
    virtual void abort() = 0;

Q_SIGNALS:
    void progress(qint64 bytesSent, qint64 bytesTotal);
    // message is what tailscaled said about a failure, empty when it said nothing
    void finished(LocalApi::Outcome outcome, const QString &message);
};

// Packs a folder into one archive file at a time. After cancel() it reports nothing more and leaves no file behind.
class Packer : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // format is the file name extension of the archive, e.g. "zip" or "tar.gz"
    virtual void pack(const QString &folder, const QString &format) = 0;
    virtual void cancel() = 0;

Q_SIGNALS:
    void progress(qint64 bytesPacked, qint64 bytesTotal);
    // archivePath is the new archive, which the receiver of the signal owns; empty with errorString set on failure
    void finished(const QString &archivePath, const QString &errorString);
};

// Counts down while an upload makes no progress; the tests replace it to control time
class Watchdog : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual void restart() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void expired();
};

// The Watchdog for real uploads
class StallTimer : public Watchdog
{
    Q_OBJECT
public:
    // Longer than the 10 s tailscaled may spend asking the device what it already has before any data flows
    static constexpr int TimeoutMs = 45000;

    explicit StallTimer(QObject *parent = nullptr);

    void restart() override;
    void stop() override;

private:
    QTimer *m_timer;
};

struct SendItem {
    enum State {
        Queued,
        Packing, // a folder being packed into an archive
        Sending,
        Finishing, // all bytes are handed to tailscaled, waiting for the device to confirm
        Done,
        Failed,
        Cancelled,
    };
    enum Failure {
        NoFailure,
        FileError, // the file could not be read, or the folder could not be packed
        NotOperator, // this user may not send files
        NodeNotFound, // the device is gone or does not accept files
        DaemonDown, // tailscaled is not running
        PeerUnreachable, // tailscaled could not reach the device
        Stalled, // no progress for StallTimer::TimeoutMs
        NotSent, // not tried, because an earlier item stopped the batch
        Other,
    };

    QString path; // the file, or the folder to pack
    QString fileName; // the name on the device; an archive of a folder is named after the folder
    bool folder = false;
    qint64 size = 0; // bytes to send; for a folder only known once it is packed
    qint64 sent = 0;
    State state = Queued;
    Failure failure = NoFailure;
    QString errorString; // details for a failure, e.g. tailscaled's message; may be empty
    QString archivePath; // the packed folder while it is being sent
};

// What one share action sends to one device
struct SendBatch {
    int id = 0;
    QString stableId;
    QString deviceName;
    QString archiveFormat; // for the folders, see Packer::pack()
    QList<SendItem> items;

    bool isFinished() const; // nothing left to do: every item is Done, Failed or Cancelled
    qint64 totalBytes() const;
    qint64 sentBytes() const;
    qsizetype doneCount() const; // the items that are Done
};

// All uploads of the helper: batches are sent first come, first served, one file at a time.
// - A file that cannot be read fails alone and the batch goes on.
// - A refusal by tailscaled (not operator, unknown device, not running) or by the device, an unreachable device or a
//   stalled upload fails the current item and marks the rest of its batch NotSent, since they would fail the same way.
// - Folders are packed right before they are sent; the archive is deleted when its item is over.
class SendQueue : public QObject
{
    Q_OBJECT
public:
    // Does not take ownership of the collaborators
    SendQueue(Transport *transport, Packer *packer, Watchdog *watchdog, QObject *parent = nullptr);
    ~SendQueue() override;

    // Returns the id of the new batch; files and folders must not both be empty
    int enqueue(const QString &stableId, const QString &deviceName, const QStringList &files, const QStringList &folders, const QString &archiveFormat);
    // Stops the batch; its items that are not Done yet become Cancelled
    void cancel(int batchId);
    // Queues the Failed and Cancelled items of a finished batch again, behind all other batches
    void retry(int batchId);
    // Drops a finished batch
    void forget(int batchId);

    // nullptr for an unknown id
    const SendBatch *batch(int batchId) const;
    // How many unfinished batches are before this one
    int batchesAhead(int batchId) const;
    bool isBusy() const;

Q_SIGNALS:
    void batchChanged(int batchId); // state or progress of its items
    void packingProgress(int batchId, qint64 bytesPacked, qint64 bytesTotal);
    void batchFinished(int batchId);

private:
    SendBatch *find(int batchId);
    SendItem &activeItem();
    void startNext();
    void upload();
    // Stops packing or sending the active item; it stays active
    void stopActive();
    // Ends the active item; removes its archive
    void endActive(SendItem::State state, SendItem::Failure failure, const QString &errorString);
    // Ends the items of the batch that are still Queued
    void endRest(int batchId, SendItem::State state, SendItem::Failure failure);
    void finishIfDone(int batchId);
    void onPacked(const QString &archivePath, const QString &errorString);
    void onProgress(qint64 bytesSent, qint64 bytesTotal);
    void onSent(LocalApi::Outcome outcome, const QString &message);

    Transport *m_transport;
    Packer *m_packer;
    Watchdog *m_watchdog;
    QList<SendBatch> m_batches; // in the order they are served
    int m_nextId = 1;
    // the item being packed or sent, if any
    int m_activeBatch = 0;
    int m_activeIndex = -1;
};
