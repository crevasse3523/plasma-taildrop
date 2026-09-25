# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Device list and share summary for the Share dialog (taildropplugin_config.qml)
ecm_add_qml_module(plasmataildropqml URI "org.crevasse3523.plasmataildrop" GENERATE_PLUGIN_SOURCE)
target_sources(plasmataildropqml PRIVATE
    filesummary.h filesummary.cpp
    tailscaletargets.h tailscaletargets.cpp)
target_link_libraries(plasmataildropqml PRIVATE
    tailscalecore Qt6::Core Qt6::Network Qt6::Qml KF6::CoreAddons KF6::I18n)
ecm_finalize_qml_module(plasmataildropqml DESTINATION ${KDE_INSTALL_QMLDIR})
