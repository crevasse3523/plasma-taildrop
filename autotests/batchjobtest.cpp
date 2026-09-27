// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// What the job view shows of a batch: amounts, status messages and speed, over a queue with faked collaborators

#include "batchjob.h"
#include "fakesendparts.h"

#include <KLocalizedString>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

// Short enough for the tests, and far below the one second between speed updates
static constexpr int QuietMs = 100;

class BatchJobTest : public QObject, public SendQueueFixture
{
    Q_OBJECT

    // Not started yet, so that spies can be connected first
    BatchJob *newJob(int batchId, int quietMs = BatchJob::QuietMs)
    {
        return new BatchJob(m_queue, batchId, m_queue, quietMs);
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

    static inline const QString NotReplying = u"Device not replying, still trying…"_s;

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
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 10), file(u"a2"_s, 20)}, {}, {});
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

    void quietAfterProgress()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 100)}, {}, {});
        BatchJob *job = newJob(id, QuietMs);
        QSignalSpy info(job, &KJob::infoMessage);
        QSignalSpy speed(job, &KJob::speed);
        job->start();
        Q_EMIT m_transport->progress(10, 100);
        // within the first second, before any speed update
        QTRY_COMPARE_WITH_TIMEOUT(lastInfo(info), NotReplying, 5 * QuietMs);
        QCOMPARE(speed.last().at(1).toULongLong(), 0);
    }

    void samePackedNameStartsAtZero()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {}, {m_dir.filePath(u"2024/Photos"_s), m_dir.filePath(u"2025/Photos"_s)}, u"zip"_s);
        BatchJob *job = newJob(id);
        QSignalSpy info(job, &KJob::infoMessage);
        job->start();
        Q_EMIT m_packer->progress(20, 20);
        QCOMPARE(lastInfo(info), u"Packing… 100%"_s);
        Q_EMIT m_packer->finished(file(u"packed.tmp"_s), {});
        finish();
        QCOMPARE(m_packer->folders.size(), 2);
        QCOMPARE(lastInfo(info), u"Packing… 0%"_s);
    }

    // the speed starts when the batch starts sending, not when it is queued
    void failedPackingStartsNextAtZero()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {}, {m_dir.filePath(u"2024/Photos"_s), m_dir.filePath(u"2025/Photos"_s)}, u"zip"_s);
        BatchJob *job = newJob(id);
        QSignalSpy info(job, &KJob::infoMessage);
        job->start();
        Q_EMIT m_packer->progress(12, 20);
        QCOMPARE(lastInfo(info), u"Packing… 60%"_s);
        Q_EMIT m_packer->finished({}, u"Permission denied"_s);
        QCOMPARE(m_packer->folders.size(), 2);
        QCOMPARE(lastInfo(info), u"Packing… 0%"_s);
    }

    void waitInQueueIsNotSpeed()
    {
        m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000)}, {}, {});
        const int id = m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s, 1000)}, {}, {});
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
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000), file(u"a2"_s, 1000)}, {}, {});
        finish();
        finish(LocalApi::Outcome::Other);
        m_queue->enqueue(u"nB"_s, u"beta"_s, {file(u"b1"_s, 1000)}, {}, {});
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
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s, 1000), file(u"a2"_s, 1000)}, {}, {});
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

    void nextFileWaitsItsOwnQuiet()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s), file(u"a2"_s)}, {}, {});
        BatchJob *job = newJob(id, QuietMs);
        QSignalSpy info(job, &KJob::infoMessage);
        job->start();
        QTRY_COMPARE(lastInfo(info), NotReplying);
        finish(LocalApi::Outcome::Other);
        QCOMPARE(lastInfo(info), QString());
    }

    void quietWhileFinishing()
    {
        const int id = m_queue->enqueue(u"nA"_s, u"alpha"_s, {file(u"a1"_s)}, {}, {});
        BatchJob *job = newJob(id, QuietMs);
        QSignalSpy info(job, &KJob::infoMessage);
        job->start();
        Q_EMIT m_transport->progress(10, 10);
        QCOMPARE(lastInfo(info), u"Finishing…"_s);
        QTRY_COMPARE(lastInfo(info), NotReplying);
    }
};

QTEST_GUILESS_MAIN(BatchJobTest)
#include "batchjobtest.moc"
