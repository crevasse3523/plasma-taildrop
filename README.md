# plasma-taildrop

A KDE [Purpose](https://invent.kde.org/frameworks/purpose) plugin that adds **Send via Tailscale…** to the
Share menu of Dolphin, Gwenview, Spectacle and other KDE apps, and to the Dolphin context menu of folders. It sends
the selected files to a device in your tailnet with [Taildrop](https://tailscale.com/kb/1106/taildrop), talking
directly to the local `tailscaled`.

Your user must be the Tailscale operator to send files: `sudo tailscale set --operator=$USER`.

## Building

Needs KDE Frameworks 6 and Qt 6.8 or newer.

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

## Development

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --build build --target clang-format
./Messages.sh                                 # update po/ after changing translatable strings
reuse lint
```

### Debian package

```sh
sudo apt build-dep ./
dpkg-buildpackage -us -uc -b
```

`scripts/release.sh X.Y.Z` sets the version, opens `debian/changelog` for the notes, commits and tags `vX.Y.Z`.

## License

[GPL-2.0-or-later](LICENSE).
