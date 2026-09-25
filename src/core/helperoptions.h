// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLatin1StringView>

// Options of plasma-taildrop-send, which the Share plugin starts. The plugin joins each value to its option with
// "=", because QGuiApplication would take a value such as -platform as an option of its own.
namespace HelperOptions
{
// StableID of the device to send to
inline constexpr QLatin1StringView DeviceId("device-id");
// Name of that device, as the job view and the notifications show it
inline constexpr QLatin1StringView DeviceName("device-name");
}
