# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Talking to tailscaled and checking the shared files; shared by the plugin, the QML module and the tests.
# No translations in the library: urlproblemtext.h is header only, for callers that link KF6::I18n.
add_library(tailscalecore STATIC
    files.cpp files.h
    helperoptions.h
    localapi.cpp localapi.h
    settings.h
    targets.cpp targets.h
    urlproblemtext.h)
set_target_properties(tailscalecore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tailscalecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tailscalecore PUBLIC Qt6::Core Qt6::Network)
