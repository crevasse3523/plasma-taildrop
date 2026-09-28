#!/bin/sh
# SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Usage: scripts/release.sh X.Y.Z
# Sets the version in CMakeLists.txt, opens a new debian/changelog entry in $EDITOR for the release notes, commits
# both and creates the annotated tag vX.Y.Z, all as the Maintainer of debian/control, whose name and email git config
# user.name and user.email must be. When both files already have that version, it only tags. Never pushes.
# Needs git and devscripts (dch).
set -eu
cd "$(dirname "$0")/.."

version=${1:-}
if ! echo "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "usage: $0 X.Y.Z" >&2
    exit 2
fi
if [ -n "$(git status --porcelain)" ]; then
    echo "The working tree has changes; commit or stash them first." >&2
    exit 1
fi
if git rev-parse -q --verify "refs/tags/v$version" >/dev/null; then
    echo "Tag v$version already exists." >&2
    exit 1
fi

# the release commit, the tag and the changelog entry are all made as the maintainer of the package
maintainer=$(sed -n 's/^Maintainer: //p' debian/control)
maintainer_name=${maintainer% <*}
maintainer_email=$(echo "$maintainer" | sed 's/.*<\(.*\)>.*/\1/')
if [ "$(git config user.name) <$(git config user.email)>" != "$maintainer" ]; then
    echo "git config user.name and user.email are not $maintainer, the Maintainer in debian/control." >&2
    exit 1
fi

# .github/workflows/package.yml reads the version with the same sed
cmake_version() {
    sed -n 's/^project(plasma-taildrop VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt
}

if [ "$(cmake_version)" != "$version" ]; then
    if ! dpkg --compare-versions "$version" gt "$(dpkg-parsechangelog -SVersion)"; then
        echo "$version is not newer than $(dpkg-parsechangelog -SVersion)." >&2
        exit 1
    fi
    DEBFULLNAME=$maintainer_name DEBEMAIL=$maintainer_email \
        dch --newversion "$version" --distribution unstable --urgency medium
    # dch succeeds even when the editor was closed without saving the entry
    if [ "$(dpkg-parsechangelog -SVersion)" != "$version" ]; then
        git checkout -- debian/changelog
        echo "debian/changelog has no entry for $version; nothing was changed." >&2
        exit 1
    fi
    sed -i "s/^project(plasma-taildrop VERSION [0-9.]*/project(plasma-taildrop VERSION $version/" CMakeLists.txt
    git add CMakeLists.txt debian/changelog
    git commit -m "Release $version"
fi

# CI refuses to publish a tag that differs from either file
if [ "$(cmake_version)" != "$version" ] || [ "$(dpkg-parsechangelog -SVersion)" != "$version" ]; then
    echo "CMakeLists.txt ($(cmake_version)) and debian/changelog ($(dpkg-parsechangelog -SVersion)) must both be $version." >&2
    exit 1
fi
git tag -a "v$version" -m "plasma-taildrop $version"
echo "Tagged v$version. Publish it with: git push --follow-tags"
