// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The order and the error handling of the send queue, with every collaborator faked and no real time passing

#include "fakesendparts.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class SendQueueTest : public QObject, public SendQueueFixture
{
    Q_OBJECT

    QStringList sentNames() const
    {
        QStringList names;
        for (const FakeTransport::Send &send : std::as_const(m_transport->sends)) {
            names.append(send.fileName);
        }
        return names;
    }

    const SendItem &item(int batchId, int index) const
    {
        return m_queue->batch(batchId)->items.at(index);
    }

private Q_SLOTS:
    void init()
    {
        initQueue();
    }

    void cleanup()
    {
        cleanupQueue();
    }

    void firstComeFirstServed()
    {
        QSignalSpy finished(m_queue, &SendQueue::batchFinished);
        const int first = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s)});
        const int second = m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s)});
        QVERIFY(m_queue->isBusy());
        QCOMPARE(sentNames(), QStringList{u"a1"_s});
        QCOMPARE(m_transport->sends.first().stableId, u"nA"_s);
        QCOMPARE(m_transport->sends.first().filePath, m_dir.filePath(u"a1"_s));
        QCOMPARE(item(first, 0).state, SendItem::Sending);
        QCOMPARE(item(first, 1).state, SendItem::Queued);
        QCOMPARE(m_queue->batchesAhead(first), 0);
        QCOMPARE(m_queue->batchesAhead(second), 1);

        finish();
        QCOMPARE(sentNames(), (QStringList{u"a1"_s, u"a2"_s}));
        QCOMPARE(item(first, 0).state, SendItem::Done);
        QVERIFY(finished.isEmpty());

        finish();
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toInt(), first);
        QCOMPARE(m_queue->batchesAhead(second), 0);
        QCOMPARE(sentNames(), (QStringList{u"a1"_s, u"a2"_s, u"b1"_s}));
        QCOMPARE(m_transport->sends.last().stableId, u"nB"_s);

        finish();
        QCOMPARE(finished.size(), 2);
        QVERIFY(!m_queue->isBusy());
    }

    void totals()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"small"_s, 10), file(u"big"_s, 1000)});
        const SendBatch *batch = m_queue->batch(id);
        QCOMPARE(batch->deviceName, u"alpha"_s);
        QCOMPARE(batch->totalBytes(), 1010);
        QCOMPARE(batch->sentBytes(), 0);
        QCOMPARE(batch->doneCount(), 0);
        Q_EMIT m_transport->progress(4, 10);
        QCOMPARE(batch->sentBytes(), 4);
        finish();
        QCOMPARE(batch->sentBytes(), 10);
        QCOMPARE(batch->doneCount(), 1);
        QVERIFY(!batch->isFinished());
    }

    // the file grew between the share and its send
    void sizeAtSend()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 10)});
        const SendBatch *batch = m_queue->batch(id);
        Q_EMIT m_transport->progress(30, 30);
        QCOMPARE(batch->totalBytes(), 30);
        QCOMPARE(batch->sentBytes(), 30);
        finish();
        QCOMPARE(batch->totalBytes(), 30);
        QCOMPARE(batch->sentBytes(), 30);
    }

    void unreadableFileIsSkipped()
    {
        const QString broken = file(u"broken"_s);
        m_transport->unreadable = {broken};
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {broken, file(u"good"_s)});
        QCOMPARE(item(id, 0).state, SendItem::Failed);
        QCOMPARE(item(id, 0).failure, SendItem::FileError);
        QCOMPARE(item(id, 0).errorString, u"Permission denied"_s);
        QCOMPARE(sentNames(), QStringList{u"good"_s});
        finish();
        QVERIFY(m_queue->batch(id)->isFinished());
        QCOMPARE(item(id, 1).state, SendItem::Done);
        QCOMPARE(m_queue->batch(id)->doneCount(), 1);
    }

    void onlyUnreadableFiles()
    {
        QSignalSpy finished(m_queue, &SendQueue::batchFinished);
        const QString broken = file(u"broken"_s);
        m_transport->unreadable = {broken};
        const int first = m_queue->enqueue(u"nA"_s, u"alpha"_s, {broken});
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toInt(), first);
        QVERIFY(m_queue->batch(first)->isFinished());
        // the next batch is not held up
        m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s)});
        QCOMPARE(sentNames(), QStringList{u"b1"_s});
    }

    void fileRejectedByDeviceIsSkipped()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s)});
        finish(LocalApi::Outcome::Other, u"invalid filename"_s);
        QCOMPARE(item(id, 0).failure, SendItem::Other);
        QCOMPARE(item(id, 0).errorString, u"invalid filename"_s);
        QCOMPARE(item(id, 1).state, SendItem::Sending);
    }

    void refusalStopsBatch_data()
    {
        QTest::addColumn<LocalApi::Outcome>("outcome");
        QTest::addColumn<SendItem::Failure>("failure");
        QTest::newRow("not operator") << LocalApi::Outcome::NotOperator << SendItem::NotOperator;
        QTest::newRow("unknown device") << LocalApi::Outcome::NodeNotFound << SendItem::NodeNotFound;
        QTest::newRow("daemon down") << LocalApi::Outcome::DaemonDown << SendItem::DaemonDown;
        QTest::newRow("unreachable") << LocalApi::Outcome::PeerUnreachable << SendItem::PeerUnreachable;
    }

    void refusalStopsBatch()
    {
        QFETCH(LocalApi::Outcome, outcome);
        QFETCH(SendItem::Failure, failure);
        QSignalSpy finished(m_queue, &SendQueue::batchFinished);
        const int first = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s), file(u"a3"_s)});
        m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s)});
        finish(outcome, u"no"_s);
        QCOMPARE(item(first, 0).state, SendItem::Failed);
        QCOMPARE(item(first, 0).failure, failure);
        QCOMPARE(item(first, 0).errorString, u"no"_s);
        for (int index : {1, 2}) {
            QCOMPARE(item(first, index).state, SendItem::Failed);
            QCOMPARE(item(first, index).failure, SendItem::NotSent);
        }
        QCOMPARE(finished.size(), 1);
        QCOMPARE(sentNames(), (QStringList{u"a1"_s, u"b1"_s}));
    }

    // the device's message is the reason; it is not about the operator
    void deviceRefusalStopsBatch()
    {
        QSignalSpy finished(m_queue, &SendQueue::batchFinished);
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s), file(u"a3"_s)});
        finish(LocalApi::Outcome::DeviceRefused, u"Taildrop disabled; no storage directory"_s);
        QCOMPARE(item(id, 0).state, SendItem::Failed);
        QCOMPARE(item(id, 0).failure, SendItem::Other);
        QCOMPARE(item(id, 0).errorString, u"Taildrop disabled; no storage directory"_s);
        for (int index : {1, 2}) {
            QCOMPARE(item(id, index).state, SendItem::Failed);
            QCOMPARE(item(id, index).failure, SendItem::NotSent);
        }
        QCOMPARE(sentNames(), QStringList{u"a1"_s});
        QCOMPARE(finished.size(), 1);
    }

    void progressAndFinishing()
    {
        QSignalSpy changed(m_queue, &SendQueue::batchChanged);
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 100)});
        changed.clear();

        Q_EMIT m_transport->progress(40, 100);
        QCOMPARE(item(id, 0).sent, 40);
        QCOMPARE(item(id, 0).state, SendItem::Sending);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first(), QVariantList{id});

        // the same count again is no progress
        Q_EMIT m_transport->progress(40, 100);
        QCOMPARE(changed.size(), 1);

        Q_EMIT m_transport->progress(100, 100);
        QCOMPARE(item(id, 0).state, SendItem::Finishing);

        finish();
        QCOMPARE(item(id, 0).state, SendItem::Done);
        QCOMPARE(item(id, 0).sent, 100);
    }

    void cancelActiveBatch()
    {
        QSignalSpy finished(m_queue, &SendQueue::batchFinished);
        const int first = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s)});
        const int second = m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s)});
        finish();
        m_queue->cancel(first);
        QCOMPARE(m_transport->aborts, 1);
        QCOMPARE(item(first, 0).state, SendItem::Done);
        QCOMPARE(item(first, 1).state, SendItem::Cancelled);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toInt(), first);
        QCOMPARE(sentNames(), (QStringList{u"a1"_s, u"a2"_s, u"b1"_s}));
        QCOMPARE(m_queue->batchesAhead(second), 0);

        // cancelling again or a finished batch changes nothing
        m_queue->cancel(first);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(m_transport->aborts, 1);
    }

    void cancelQueuedBatch()
    {
        const int first = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s)});
        const int second = m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s)});
        const int third = m_queue->enqueue(u"nC"_s, u"gamma"_s, {file(u"c1"_s)});
        m_queue->cancel(second);
        QCOMPARE(m_transport->aborts, 0);
        QCOMPARE(item(first, 0).state, SendItem::Sending);
        QCOMPARE(item(second, 0).state, SendItem::Cancelled);
        QCOMPARE(m_queue->batchesAhead(third), 1);
        finish();
        QCOMPARE(sentNames(), (QStringList{u"a1"_s, u"c1"_s}));
    }
};

QTEST_GUILESS_MAIN(SendQueueTest)
#include "sendqueuetest.moc"
