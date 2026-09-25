// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Share dialog page: what is sent and to which device.
// i18nd() comes from the KLocalizedContext of the sharing application, which qmllint cannot see.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD

import org.crevasse3523.plasmataildrop

ColumnLayout {
    id: root

    // Set by Purpose: the shared URLs and their common MIME type, which is not needed here
    property list<string> urls
    property string mimeType
    // Read by Purpose: StableID of the device to send to. Undefined keeps Send disabled until the share can go out.
    property var device: chosen !== "" && canPick ? chosen : undefined
    // Read by Purpose: the name the job view shows for that device; defined exactly when device is
    property var deviceName: device !== undefined ? targets.nameOf(chosen) : undefined

    readonly property var summary: FileSummary.summarize(urls)
    property string chosen
    readonly property bool canPick: summary.problem === "" && !summary.hasFolders

    TailscaleTargetsModel {
        id: targets
        // a device chosen before Refresh may have gone offline or away
        onLoaded: {
            if (!targets.isOnline(root.chosen)) {
                root.chosen = ""
            }
        }
    }

    RowLayout {
        Kirigami.Heading {
            Layout.fillWidth: true
            text: root.summary.text
            level: 2
            wrapMode: Text.Wrap
        }

        QQC2.ToolButton {
            icon.name: "view-refresh"
            text: i18nd("plasma-taildrop", "Refresh") // qmllint disable unqualified
            display: QQC2.AbstractButton.IconOnly
            enabled: !targets.loading
            onClicked: targets.reload()

            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        text: root.summary.problem
        visible: text !== ""
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        visible: root.summary.hasFolders
        text: i18nd("plasma-taildrop", "Only files can be sent, not folders.") // qmllint disable unqualified
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        // room for the placeholder message when there is nothing to list
        Layout.minimumHeight: Kirigami.Units.gridUnit * 4

        ListView {
            id: list

            clip: true
            model: targets
            enabled: root.canPick

            delegate: KD.RadioSubtitleDelegate {
                required property string stableId
                required property string name
                required property string ip
                required property string os
                required property bool online

                width: ListView.view.width
                enabled: online
                text: name
                subtitle: online ? ip : i18nd("plasma-taildrop", "offline") // qmllint disable unqualified
                icon.name: os === "android" || os === "iOS" ? "smartphone" : os === "" ? "network-vpn" : "computer"
                checked: root.chosen === stableId
                onClicked: root.chosen = stableId
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.largeSpacing * 4
                visible: list.count === 0
                text: targets.loading ? i18nd("plasma-taildrop", "Loading devices…") // qmllint disable unqualified
                    : targets.error !== "" ? targets.error
                    : i18nd("plasma-taildrop", "No devices found") // qmllint disable unqualified
            }
        }
    }
}
