// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sendservice.h"
#include "batchjob.h"
#include "helperoptions.h"
#include "localapitransport.h"
#include "notifier.h"
#include "sendqueue.h"
#include "settings.h"

#include <KLocalizedString>
#include <KUiServerV2JobTracker>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>

using namespace Qt::StringLiterals;

SendService::SendService(QObject *parent)
    : QObject(parent)
    , m_queue(new SendQueue(new LocalApiTransport(this), this))
    , m_notifier(new Notifier(this))
    , m_tracker(new KUiServerV2JobTracker(this))
{
    connect(m_queue, &SendQueue::batchFinished, this, &SendService::onBatchFinished);
    connect(m_notifier, &Notifier::retryRequested, this, [this](int batchId) {
        m_queue->retry(batchId);
        track(batchId);
        updateIdle();
    });
    connect(m_notifier, &Notifier::closed, this, [this](int batchId) {
        // does nothing when the batch is being retried
        m_queue->forget(batchId);
    });
    m_idle.setSingleShot(true);
    m_idle.setInterval(IdleMs);
    connect(&m_idle, &QTimer::timeout, this, &SendService::idle);
}

SendService::~SendService()
{
    // before the transport, which it still stops; the children go in the order they came
    delete m_queue;
}

void SendService::addOptions(QCommandLineParser &parser)
{
    parser.addOption({HelperOptions::DeviceId, i18n("StableID of the device to send to."), i18n("id")});
    parser.addOption({HelperOptions::DeviceName, i18n("Name of the device to send to."), i18n("name")});
    parser.addPositionalArgument(u"files"_s, i18n("Files to send."), i18n("[files…]"));
}

void SendService::handle(const QStringList &arguments, const QString &workingDirectory)
{
    QCommandLineParser parser;
    addOptions(parser);
    const QDir directory(workingDirectory);
    QString stableId;
    QStringList files;
    if (parser.parse(arguments)) {
        stableId = parser.value(HelperOptions::DeviceId);
        const QStringList paths = parser.positionalArguments();
        for (const QString &path : paths) {
            files.append(directory.absoluteFilePath(path));
        }
    }
    if (stableId.isEmpty() || files.isEmpty()) {
        qWarning().noquote() << "Ignoring a launch without a device or files:" << arguments.join(u' ');
    } else {
        const QString name = parser.value(HelperOptions::DeviceName);
        track(m_queue->enqueue(stableId, name.isEmpty() ? stableId : name, files));
    }
    updateIdle();
}

bool SendService::isBusy() const
{
    return m_queue->isBusy();
}

void SendService::track(int batchId)
{
    const SendBatch *batch = m_queue->batch(batchId);
    // a batch that ended at once, e.g. on unreadable files, would leave its job hanging
    if (!batch || batch->isFinished()) {
        return;
    }
    auto job = new BatchJob(m_queue, batchId, this);
    m_tracker->registerJob(job);
    job->start();
}

void SendService::onBatchFinished(int batchId)
{
    const SendBatch *batch = m_queue->batch(batchId);
    if (batch->doneCount() > 0) {
        // preselected in the Share dialog next time
        QSettings(Settings::Name).setValue(Settings::LastTargetIdKey, batch->stableId);
    }
    m_notifier->notify(*batch);
    updateIdle();
}

void SendService::updateIdle()
{
    if (m_queue->isBusy()) {
        m_idle.stop();
    } else if (!m_idle.isActive()) {
        m_idle.start();
    }
}
