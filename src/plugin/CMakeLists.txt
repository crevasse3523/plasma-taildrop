# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

configure_file(taildropplugin.json.in taildropplugin.json @ONLY)

# Share menu entry: hands the files over to plasma-taildrop-send
kcoreaddons_add_plugin(taildropplugin
    SOURCES taildropplugin.cpp
    INSTALL_NAMESPACE "kf6/purpose")
target_compile_definitions(taildropplugin PRIVATE
    PLASMA_TAILDROP_HELPER_PATH="${KDE_INSTALL_FULL_LIBEXECDIR}/$<TARGET_FILE_NAME:plasma-taildrop-send>")
target_link_libraries(taildropplugin tailscalecore Qt6::Core KF6::CoreAddons KF6::I18n KF6::Purpose)

install(FILES taildropplugin_config.qml DESTINATION ${KDE_INSTALL_DATADIR}/kf6/purpose)
