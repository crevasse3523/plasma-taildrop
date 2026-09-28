# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

configure_file(taildropfileitemaction.json.in taildropfileitemaction.json @ONLY)

# Dolphin context menu entry for folders: opens the Share dialog of taildropplugin
kcoreaddons_add_plugin(taildropfileitemaction
    SOURCES taildropfileitemaction.cpp
    INSTALL_NAMESPACE "kf6/kfileitemaction")
target_link_libraries(taildropfileitemaction KF6::I18n KF6::KIOWidgets KF6::Notifications KF6::Purpose KF6::PurposeWidgets)
# the ID of the Share plugin whose entry the folder menu shows
target_compile_definitions(taildropfileitemaction PRIVATE SHARE_PLUGIN_ID="$<TARGET_FILE_BASE_NAME:taildropplugin>")
