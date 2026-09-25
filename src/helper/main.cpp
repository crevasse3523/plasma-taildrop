// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// plasma-taildrop-send: the Share menu entry hands the files over to this program, so that they are sent after
// the dialog closed and even after the application that shared them quit. Only one runs: later launches pass their
// files to it and exit, so everything shared goes through one queue.

#include "sendservice.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    // a finished KJob would otherwise quit the application; SendService decides when to quit
    QCoreApplication::setQuitLockEnabled(false);
    KLocalizedString::setApplicationDomain(TRANSLATION_DOMAIN);

    KAboutData about(u"plasma-taildrop-send"_s,
                     i18n("Send via Tailscale"),
                     QStringLiteral(PROJECT_VERSION),
                     i18n("Sends files to the devices of your tailnet with Taildrop"),
                     KAboutLicense::GPL_V2,
                     i18n("© 2026 crevasse3523"));
    about.addAuthor(u"crevasse3523"_s, {}, u"335460626+crevasse3523@users.noreply.github.com"_s);
    about.setHomepage(u"https://github.com/crevasse3523/plasma-taildrop"_s);
    // the D-Bus name of the single instance is org.crevasse3523.plasma-taildrop-send
    about.setOrganizationDomain("crevasse3523.org");
    // gives the job view and the notifications their name and icon
    about.setDesktopFileName(QStringLiteral(HELPER_DESKTOP_ID));
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    SendService::addOptions(parser);
    parser.process(app);
    about.processCommandLine(&parser);

    // without a session bus every launch sends on its own
    KDBusService service(KDBusService::Unique | KDBusService::NoExitOnFailure);
    SendService sender;
    QObject::connect(&service, &KDBusService::activateRequested, &sender, &SendService::handle);
    QObject::connect(&sender, &SendService::idle, &service, [&service, &sender] {
        // the name stays ours until the process ends, and a launch accepted meanwhile would be lost
        service.unregister();
        // a launch that reached us before the name was released comes first
        QMetaObject::invokeMethod(
            &sender,
            [&sender] {
                if (!sender.isBusy()) {
                    QCoreApplication::quit();
                }
            },
            Qt::QueuedConnection);
    });
    sender.handle(app.arguments(), QDir::currentPath());
    return app.exec();
}
