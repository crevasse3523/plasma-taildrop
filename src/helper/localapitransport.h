// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sendqueue.h"

class QNetworkAccessManager;
class QNetworkReply;

// Sends a file with PUT file-put, streamed from disk. Only HTTP 200 counts as sent: the IPN bus of tailscaled
// reports success even for some failed transfers.
class LocalApiTransport : public Transport
{
    Q_OBJECT
public:
    explicit LocalApiTransport(QObject *parent = nullptr);

    bool send(const QString &stableId, const QString &filePath, const QString &fileName, QString *errorString) override;
    void abort() override;

private:
    void release();

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
};
