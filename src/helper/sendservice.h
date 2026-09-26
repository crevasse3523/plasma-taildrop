// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

class KUiServerV2JobTracker;
class QCommandLineParser;
class Notifier;
class SendQueue;

// Everything plasma-taildrop-send does: takes the files of every launch into one queue, shows each batch in the
// job view and its result as a notification, and says when it has had nothing to do for a while.
class SendService : public QObject
{
    Q_OBJECT
public:
    // How long to stay after the queue ran empty, which is also how long Retry in a notification works
    static constexpr int IdleMs = 60000;

    explicit SendService(QObject *parent = nullptr);
    ~SendService() override;

    // The options of the command line: --device-id=ID [--device-name=NAME] -- FILE...
    static void addOptions(QCommandLineParser &parser);

    // Queues what a launch asks for; arguments starts with the program, relative paths are in workingDirectory
    void handle(const QStringList &arguments, const QString &workingDirectory);

    // Whether a batch is still waiting or being sent
    bool isBusy() const;

Q_SIGNALS:
    // IdleMs after the last batch finished
    void idle();

private:
    void track(int batchId);
    void onBatchFinished(int batchId);
    void updateIdle();

    SendQueue *m_queue;
    Notifier *m_notifier;
    KUiServerV2JobTracker *m_tracker;
    QTimer m_idle;
};
