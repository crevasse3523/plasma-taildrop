#!/bin/sh
# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Extracts translatable strings into po/plasma-taildrop.pot and merges them into every po/<lang>/plasma-taildrop.po.
# Run from the source directory; needs gettext.
set -e
cd "$(dirname "$0")"

xgettext --from-code=UTF-8 --language=C++ --kde \
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ki18nd:2 -ki18ndc:2c,3 -kki18n:1 \
    --package-name=plasma-taildrop --msgid-bugs-address=https://github.com/crevasse3523/plasma-taildrop/issues \
    -o po/plasma-taildrop.pot $(find src -name '*.cpp' -o -name '*.h' -o -name '*.qml' | sort)

for po in po/*/plasma-taildrop.po; do
    msgmerge --quiet --update --backup=none "$po" po/plasma-taildrop.pot
done
