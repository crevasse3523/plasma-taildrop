# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)
include(ECMAddTests)

ecm_add_tests(targetstest.cpp filestest.cpp localapitest.cpp
    LINK_LIBRARIES tailscalecore Qt6::Test)
ecm_add_tests(sendqueuetest.cpp batchjobtest.cpp archivertest.cpp localapitransporttest.cpp
    LINK_LIBRARIES sendcore Qt6::Test)
# the notifier is part of the helper executable, not of sendcore
ecm_add_test(notifiertest.cpp ../src/helper/notifier.cpp
    TEST_NAME notifiertest
    LINK_LIBRARIES sendcore Qt6::Test KF6::Notifications)

# The QML types are built into their tests directly: the QML module is only a plugin library
ecm_add_test(targetsmodeltest.cpp ../src/qml/tailscaletargets.cpp
    TEST_NAME targetsmodeltest
    LINK_LIBRARIES tailscalecore Qt6::Network Qt6::Qml Qt6::Test KF6::CoreAddons KF6::I18n)
target_include_directories(targetsmodeltest PRIVATE ../src/qml)
ecm_add_test(filesummarytest.cpp ../src/qml/filesummary.cpp
    TEST_NAME filesummarytest
    LINK_LIBRARIES sendcore Qt6::Qml Qt6::Test KF6::CoreAddons KF6::I18n)
target_include_directories(filesummarytest PRIVATE ../src/qml)

# Loads the built plugin; PLASMA_TAILDROP_HELPER points it at a stub
ecm_add_test(pluginjobtest.cpp
    TEST_NAME pluginjobtest
    LINK_LIBRARIES Qt6::Test KF6::CoreAddons KF6::I18n KF6::Purpose)
target_compile_definitions(pluginjobtest PRIVATE PLUGIN_PATH="$<TARGET_FILE:taildropplugin>")
add_dependencies(pluginjobtest taildropplugin)

