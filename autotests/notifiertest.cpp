// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The notification text of a failed item, which the notification server renders as markup

#include "notifier.h"
#include "sendqueue.h"

#include <KLocalizedString>
#include <QTest>

using namespace Qt::StringLiterals;

class NotifierTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // the messages as written, whatever translation is installed
        KLocalizedString::setLanguages({u"en_US"_s});
    }

    void failureLineIsEscaped()
    {
        SendItem item;
        item.fileName = u"a<b>&c.txt"_s;
        item.state = SendItem::Failed;
        item.failure = SendItem::Other;
        item.errorString = u"<a href=x>y</a>"_s;
        QCOMPARE(failureLine(item), u"a&lt;b&gt;&amp;c.txt: &lt;a href=x&gt;y&lt;/a&gt;"_s);
    }
};

QTEST_GUILESS_MAIN(NotifierTest)

#include "notifiertest.moc"
