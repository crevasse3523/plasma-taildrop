// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Share submenu for folders. The Share menu of Dolphin offers files only, so this adds a Share menu for folders too,
// holding just Send via Tailscale…: other Share plugins are not made for folders. It opens the same dialog.
// Dolphin loads the plugin only when all selected items are folders (see the MIME types in the JSON).

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KIO/Global>
#include <KLocalizedString>
#include <KNotification>
#include <KPluginFactory>
#include <QJsonArray>
#include <purpose/alternativesmodel.h>
#include <purpose/menu.h>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

class TaildropFileItemAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
public:
    using KAbstractFileItemActionPlugin::KAbstractFileItemActionPlugin;

    ~TaildropFileItemAction() override
    {
        delete m_menu;
    }

    QList<QAction *> actions(const KFileItemListProperties &selection, QWidget *) override
    {
        // the helper sends local files only, and the Share menu has the entry for files
        const KFileItemList items = selection.items();
        if (!selection.isLocal() || std::none_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
                return item.isDir();
            })) {
            return {};
        }
        QJsonArray urls;
        for (const KFileItem &item : items) {
            urls.append(item.mostLocalUrl().toString());
        }
        const QString mimeType = selection.mimeType();
        // the menu reloads its entries for new input data
        Purpose::Menu *menu = this->menu();
        menu->model()->setInputData({{u"mimeType"_s, mimeType.isEmpty() ? u"*/*"_s : mimeType}, {u"urls"_s, urls}});

        // only our entry stays visible; newer Purpose sorts the entries, so they are told apart by plugin ID, not by row
        const QList<QAction *> entries = menu->actions();
        bool found = false;
        for (QAction *entry : entries) {
            const bool ours = entry->property("pluginId").toString() == QLatin1StringView(SHARE_PLUGIN_ID);
            entry->setVisible(ours);
            found = found || ours;
        }
        // no menu when Purpose leaves the plugin out, e.g. because it is disabled in purposerc
        return found ? QList<QAction *>{menu->menuAction()} : QList<QAction *>{};
    }

private:
    Purpose::Menu *menu()
    {
        if (m_menu) {
            return m_menu;
        }
        m_menu = new Purpose::Menu;
        // like the Share menu of files
        m_menu->setTitle(i18nc("@action:inmenu", "Share"));
        m_menu->setIcon(QIcon::fromTheme(u"document-share"_s));
        m_menu->model()->setPluginType(u"Export"_s);
        connect(m_menu, &Purpose::Menu::aboutToShare, this, [this] {
            // Dolphin deletes this plugin with its context menu, while the dialog of the menu stays open
            Purpose::Menu *menu = std::exchange(m_menu, nullptr);
            connect(menu, &Purpose::Menu::finished, menu, &QObject::deleteLater);
        });
        // shown by the menu itself: this plugin, and Dolphin's listener for error(), are gone with the context menu
        connect(m_menu, &Purpose::Menu::finished, m_menu, [](const QJsonObject &, int error, const QString &errorMessage) {
            if (error && error != KIO::ERR_USER_CANCELED) {
                KNotification::event(KNotification::Error, i18nc("@title:notification", "Sharing failed"), errorMessage.toHtmlEscaped());
            }
        });
        return m_menu;
    }

    Purpose::Menu *m_menu = nullptr; // hidden; owned by this plugin until one of its entries is triggered
};

K_PLUGIN_CLASS_WITH_JSON(TaildropFileItemAction, "taildropfileitemaction.json")

#include "taildropfileitemaction.moc"
