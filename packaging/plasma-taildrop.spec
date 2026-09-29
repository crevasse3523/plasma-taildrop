# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Fedora package. The release notes are in debian/changelog, so there is no %%changelog here.

Name:           plasma-taildrop
Version:        1.0.0
Release:        1%{?dist}
Summary:        Send files to Tailscale devices from the KDE Share menu
License:        GPL-2.0-or-later AND CC0-1.0
URL:            https://github.com/crevasse3523/plasma-taildrop
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  appstream
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  kf6-karchive-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kdbusaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kio-devel
BuildRequires:  kf6-kjobwidgets-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  kf6-purpose-devel
BuildRequires:  kf6-rpm-macros
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel

Requires:       kf6-kirigami
Requires:       kf6-purpose
Requires:       qt6-qtdeclarative
Recommends:     tailscale

%description
A KDE Purpose plugin that adds "Send via Tailscale…" to the Share menu of
Dolphin, Gwenview, Spectacle and other KDE applications, and to the Dolphin
context menu of folders. It sends the selected files to a device in your
tailnet with Taildrop.

%prep
%autosetup -n %{name}-%{version}

%build
# not %%cmake_build_kf6: it also builds API documentation, which this project has none of
%cmake_kf6 -DBUILD_TESTING=ON -DINSTALL_GIT_HOOKS=OFF
%cmake_build

%install
%cmake_install
%find_lang %{name}

%check
# The tests create QML types and a QGuiApplication, and a build has no display
export QT_QPA_PLATFORM=offscreen
%ctest

%files -f %{name}.lang
%license LICENSE LICENSES/*
%doc README.md
%{_libexecdir}/plasma-taildrop-send
%{_kf6_plugindir}/purpose/taildropplugin.so
%{_kf6_plugindir}/kfileitemaction/taildropfileitemaction.so
%dir %{_kf6_qmldir}/org/crevasse3523
%{_kf6_qmldir}/org/crevasse3523/plasmataildrop/
%{_kf6_datadir}/kf6/purpose/taildropplugin_config.qml
%{_kf6_datadir}/applications/org.crevasse3523.plasma_taildrop_send.desktop
%{_kf6_datadir}/knotifications6/plasma-taildrop.notifyrc
%{_kf6_metainfodir}/org.crevasse3523.plasma_taildrop.metainfo.xml
