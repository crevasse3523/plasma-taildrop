# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Talking to tailscaled over its LocalAPI
add_library(tailscalecore STATIC
    localapi.cpp localapi.h
    targets.cpp targets.h)
set_target_properties(tailscalecore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tailscalecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tailscalecore PUBLIC Qt6::Core Qt6::Network)
