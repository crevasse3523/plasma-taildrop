// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// What the job view shows of a batch: amounts, status messages and speed, over a queue with faked collaborators

#include "batchjob.h"
#include "fakesendparts.h"

#include <KLocalizedString>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class BatchJobTest : public QObject, public SendQueueFixture
{
    Q_OBJECT

    // Not started yet, so that spies can be connected first
    BatchJob *newJob(int batchId)
    {
        return new BatchJob(m_queue, batchId, m_queue);
    }

    static QString lastInfo(const QSignalSpy &info)
    {
        return info.isEmpty() ? QString() : info.last().at(1).toString();
    }

    static bool anySpeed(const QSignalSpy &speed)
    {
        return std::any_of(speed.cbegin(), speed.cend(), [](const QList<QVariant> &args) {
            return args.at(1).toULongLong() > 0;
        });
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({u"en_US"_s});
    }

    void init()
    {
        initQueue();
    }

    void cleanup()
    {
        cleanupQueue();
    }

    void amountsAndResult()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 10), file(u"a2"_s, 20)});
        BatchJob *job = newJob(id);
        QSignalSpy result(job, &KJob::result);
        job->start();
        QCOMPARE(job->totalAmount(KJob::Files), 2);
        QCOMPARE(job->totalAmount(KJob::Bytes), 30);

        Q_EMIT m_transport->progress(4, 10);
        QCOMPARE(job->processedAmount(KJob::Bytes), 4);
        finish();
        QCOMPARE(job->processedAmount(KJob::Files), 1);
        QVERIFY(result.isEmpty());

        finish(LocalApi::Outcome::Other);
        QCOMPARE(result.size(), 1);
        // how it went is for the notification
        QCOMPARE(job->error(), 0);
    }

    void waitInQueueIsNotSpeed()
    {
        m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000)});
        const int id = m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s, 1000)});
        BatchJob *job = newJob(id);
        QSignalSpy speed(job, &KJob::speed);
        job->start();
        QTest::qWait(1100);
        finish();
        Q_EMIT m_transport->progress(10, 1000);
        QVERIFY(!anySpeed(speed));
    }

    // the files sent before the retry are not sent again, so they are no speed
    void retriedBatchCountsOnlyNewBytes()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000), file(u"a2"_s, 1000)});
        finish();
        finish(LocalApi::Outcome::Other);
        m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s, 1000)});
        m_queue->retry(id);
        BatchJob *job = newJob(id);
        QSignalSpy speed(job, &KJob::speed);
        job->start();
        QTest::qWait(1100);
        finish();
        Q_EMIT m_transport->progress(10, 1000);
        QVERIFY(!anySpeed(speed));
        // a second later, only the bytes of a2 count, not the 1000 of a1 sent before the retry
        QTest::qWait(1100);
        Q_EMIT m_transport->progress(20, 1000);
        QVERIFY(anySpeed(speed));
        QVERIFY(speed.last().at(1).toULongLong() < 100);
    }

    // the speed goes on from one file to the next
    void speedSpansFiles()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000), file(u"a2"_s, 1000)});
        BatchJob *job = newJob(id);
        QSignalSpy speed(job, &KJob::speed);
        job->start();
        Q_EMIT m_transport->progress(1000, 1000);
        QTest::qWait(600);
        finish();
        QTest::qWait(500);
        Q_EMIT m_transport->progress(100, 1000);
        QVERIFY(anySpeed(speed));
    }

    void finishing()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s)});
        BatchJob *job = newJob(id);
        QSignalSpy info(job, &KJob::infoMessage);
        job->start();
        Q_EMIT m_transport->progress(10, 10);
        QCOMPARE(lastInfo(info), u"Finishing…"_s);
    }
};

QTEST_GUILESS_MAIN(BatchJobTest)
#include "batchjobtest.moc"
