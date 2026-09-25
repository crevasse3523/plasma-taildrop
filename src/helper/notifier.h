// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

struct SendBatch;
struct SendItem;

// The line of a failed item in the notification text, which the notification server renders as markup
QString failureLine(const SendItem &item);

// Tells how a batch ended, with the events of plasma-taildrop.notifyrc
class Notifier : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    void notify(const SendBatch &batch);
};
