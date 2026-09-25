# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(plasma-taildrop VERSION 1.0.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(KDE_COMPILERSETTINGS_LEVEL 6.0)
# Qt's QML type registration adds the QtQml private include directories, which Debian ships separately
# (qt6-declarative-private-dev); nothing here includes from them
set(KDE_SKIP_MISSING_INCLUDE_DIRS_WARNINGS_SETTINGS TRUE)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMQmlModule)
include(FeatureSummary)

# Gui: KF6::Purpose links it without looking it up itself
# 6.8: QNetworkRequest::FullLocalServerNameAttribute, for HTTP over the tailscaled unix socket
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Network)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons DBusAddons I18n JobWidgets Notifications Purpose)

add_compile_definitions(TRANSLATION_DOMAIN="plasma-taildrop")
# Name of the desktop entry of plasma-taildrop-send, which gives the job view and the notifications its name and icon
set(HELPER_DESKTOP_ID org.crevasse3523.plasma_taildrop_send)

add_subdirectory(src/core)
add_subdirectory(src/helper)
add_subdirectory(src/plugin)
add_subdirectory(src/qml)
add_subdirectory(data)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

# Only src/ and autotests/: a glob over the whole tree would also pick up build and packaging trees
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h autotests/*.cpp autotests/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

# Package builds and CI turn this off so configuring never writes into .git/hooks
option(INSTALL_GIT_HOOKS "Install a git pre-commit hook that checks the formatting" ON)
if(INSTALL_GIT_HOOKS)
    kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
