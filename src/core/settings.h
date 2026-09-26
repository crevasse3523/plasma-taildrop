// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLatin1StringView>

// Stored in ~/.config/plasma-taildrop.conf
namespace Settings
{
inline constexpr QLatin1StringView Name("plasma-taildrop");
// StableID of the device the last send went to
inline constexpr QLatin1StringView LastTargetIdKey("lastTargetId");
}
