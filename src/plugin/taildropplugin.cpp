// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Share menu entry that sends the selected files with Taildrop to the device chosen in taildropplugin_config.qml.
// Purpose runs the job inside the sharing application, so it only checks the files and hands them over to
// plasma-taildrop-send, which queues, packs and uploads them and shows the progress. Windows saves received files
// in the Downloads folder of the logged-in user.

#include "helperoptions.h"
#include "urlproblemtext.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <purpose/pluginbase.h>

using namespace Qt::StringLiterals;

class TailscaleJob : public Purpose::Job
{
    Q_OBJECT
public:
    using Purpose::Job::Job;

    void start() override
    {
        // a job reports its result only after start() returned
        QTimer::singleShot(0, this, &TailscaleJob::doStart);
    }

private:
    void doStart()
    {
        const QString device = data().value("device"_L1).toString(); // StableID
        if (device.isEmpty()) {
            fail(i18n("No device chosen"));
            return;
        }
        const ValidatedUrls validated = validateUrls(data().value("urls"_L1).toVariant().toStringList());
        if (!validated.problems.isEmpty()) {
            fail(urlProblemText(validated.problems.first()));
            return;
        }
        if (validated.files.isEmpty() && validated.folders.isEmpty()) {
            fail(i18n("No files to send"));
            return;
        }
        // the dialog asks for a format when folders are shared, and leaves it empty otherwise
        const QString archiveFormat = data().value("archiveFormat"_L1).toString();
        if (!validated.folders.isEmpty() && archiveFormat.isEmpty()) {
            fail(i18n("Folders can only be sent packed into an archive: %1", validated.folders.first()));
            return;
        }

        // one word each, see helperoptions.h
        const auto option = [](QLatin1StringView name, const QString &value) -> QString {
            return u"--"_s + name + u'=' + value;
        };
        QStringList arguments{option(HelperOptions::DeviceId, device), option(HelperOptions::DeviceName, data().value("deviceName"_L1).toString())};
        if (!validated.folders.isEmpty()) {
            arguments << option(HelperOptions::Archive, archiveFormat);
        }
        arguments << u"--"_s << validated.files << validated.folders;
        QString helper = qEnvironmentVariable("PLASMA_TAILDROP_HELPER");
        if (helper.isEmpty()) {
            helper = QStringLiteral(PLASMA_TAILDROP_HELPER_PATH);
        }
        QProcess process;
        process.setProgram(helper);
        process.setArguments(arguments);
        // the helper outlives the sharing application, and must not keep its folder busy, e.g. on a USB stick
        process.setWorkingDirectory(QDir::rootPath());
        // the reason tells a missing helper apart from, e.g., too many files for one command line
        if (!process.startDetached()) {
            fail(i18n("Could not start %1: %2", helper, process.errorString()));
            return;
        }
        emitResult();
    }

    void fail(const QString &message)
    {
        setError(KJob::UserDefinedError);
        setErrorText(message);
        emitResult();
    }
};

class TaildropPlugin : public Purpose::PluginBase
{
    Q_OBJECT
public:
    using PluginBase::PluginBase;

    Purpose::Job *createJob() const override
    {
        return new TailscaleJob(nullptr);
    }
};

K_PLUGIN_CLASS_WITH_JSON(TaildropPlugin, "taildropplugin.json")

#include "taildropplugin.moc"
