# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# The send queue, the upload and the job view of plasma-taildrop-send, in a library of their own for the tests
add_library(sendcore STATIC
    batchjob.cpp batchjob.h
    localapitransport.cpp localapitransport.h
    sendqueue.cpp sendqueue.h)
set_target_properties(sendcore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sendcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sendcore PUBLIC tailscalecore Qt6::Core Qt6::Network KF6::CoreAddons KF6::I18n)

# Started by the plugin with the files to send; not meant to be run by hand
add_executable(plasma-taildrop-send
    main.cpp
    notifier.cpp notifier.h
    sendservice.cpp sendservice.h)
target_compile_definitions(plasma-taildrop-send PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
    HELPER_DESKTOP_ID="${HELPER_DESKTOP_ID}")
target_link_libraries(plasma-taildrop-send
    sendcore Qt6::Gui KF6::CoreAddons KF6::DBusAddons KF6::I18n KF6::JobWidgets KF6::Notifications)
install(TARGETS plasma-taildrop-send DESTINATION ${KDE_INSTALL_LIBEXECDIR})
