# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Name and icon of plasma-taildrop-send in the job view and in notifications
configure_file(plasma-taildrop-send.desktop.in ${HELPER_DESKTOP_ID}.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${HELPER_DESKTOP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
configure_file(plasma-taildrop.notifyrc.in plasma-taildrop.notifyrc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plasma-taildrop.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# Lists the plugin in software centers as an addon of Dolphin
install(FILES org.crevasse3523.plasma_taildrop.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
