# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)
include(ECMAddTests)

ecm_add_tests(targetstest.cpp localapitest.cpp
    LINK_LIBRARIES tailscalecore Qt6::Test)
