// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The context menu entry of the built plugin: for which selections it is offered.

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QAction>
#include <QMenu>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <sys/stat.h>

using namespace Qt::StringLiterals;

class FileItemActionTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<KAbstractFileItemActionPlugin> m_plugin;

    // The entries offered for these items; a trailing slash marks a folder
    QList<QAction *> actions(const QStringList &urls)
    {
        KFileItemList items;
        for (const QString &url : urls) {
            const bool folder = url.endsWith(u'/');
            items.append(KFileItem(QUrl(url), folder ? u"inode/directory"_s : u"text/plain"_s, folder ? S_IFDIR : S_IFREG));
        }
        return m_plugin->actions(KFileItemListProperties(items), nullptr);
    }

    QString local(const QString &name) const
    {
        return QUrl::fromLocalFile(m_dir.filePath(name)).toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QStandardPaths::setTestModeEnabled(true); // for purposerc
        // the entries as written, whatever translation is installed
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); // the plugin metadata
        KLocalizedString::setLanguages({u"en_US"_s}); // the Share menu
        // ahead of any installed copy of the Share plugin
        QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
        const auto result = KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(KPluginMetaData(QStringLiteral(PLUGIN_PATH)));
        QVERIFY2(result, qPrintable(result.errorText));
        m_plugin.reset(result.plugin);
    }

    void offered_data()
    {
        QTest::addColumn<QStringList>("urls");
        QTest::addColumn<bool>("offered");
        QTest::newRow("files") << QStringList{local(u"a.txt"_s), local(u"b.txt"_s)} << false;
        QTest::newRow("folder") << QStringList{local(u"folder/"_s)} << true;
        QTest::newRow("folders") << QStringList{local(u"one/"_s), local(u"two/"_s)} << true;
        QTest::newRow("file and folder") << QStringList{local(u"a.txt"_s), local(u"folder/"_s)} << true;
        QTest::newRow("remote folder") << QStringList{u"sftp://example.org/folder/"_s} << false;
        QTest::newRow("local and remote folder") << QStringList{local(u"folder/"_s), u"sftp://example.org/folder/"_s} << false;
    }

    void offered()
    {
        QFETCH(QStringList, urls);
        QFETCH(bool, offered);
        const QList<QAction *> offeredActions = actions(urls);
        QCOMPARE(offeredActions.size(), offered ? 1 : 0);
        if (offered) {
            // a Share submenu holding only our entry
            QAction *share = offeredActions.first();
            QCOMPARE(share->text(), u"Share"_s);
            QVERIFY(share->menu());
            QStringList visible;
            const QList<QAction *> entries = share->menu()->actions();
            for (QAction *entry : entries) {
                if (entry->isVisible()) {
                    visible.append(entry->text());
                }
            }
            QCOMPARE(visible, QStringList{u"Send via Tailscale…"_s});
        }
    }

    void disabled()
    {
        const QString purposerc = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/purposerc"_s;
        QFile file(purposerc);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[plugins]\ndisabled=taildropplugin\n");
        file.close();
        // another selection, as Purpose looks for its plugins again only for new input
        const qsizetype count = actions({local(u"another folder/"_s)}).size();
        QFile::remove(purposerc);
        QCOMPARE(count, 0);
    }
};

QTEST_MAIN(FileItemActionTest)

#include "fileitemactiontest.moc"
