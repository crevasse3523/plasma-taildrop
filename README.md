# plasma-taildrop

A KDE [Purpose](https://invent.kde.org/frameworks/purpose) plugin that adds **Send via Tailscale…** to the
Share menu of Dolphin, Gwenview, Spectacle and other KDE apps, and to the Dolphin context menu of folders. It sends
the selected files to a device in your tailnet with [Taildrop](https://tailscale.com/kb/1106/taildrop), talking
directly to the local `tailscaled`.

## Features

- **Progress in Plasma.** Each share shows up in the Plasma job view, like a copy in Dolphin: files and bytes
  sent, speed, the file being sent, and a Cancel button. A notification reports the result.
- **One queue.** Shares from all apps go into one queue and are sent one at a time, so they never fight over the
  link. Sending carries on in the background after you close the app you shared from.
- **Cancel and Retry.** Cancelling stops the current file and skips the rest of that share. When a share fails or
  is cancelled, the notification offers **Retry**, which sends again only the files that did not arrive. Tailscale
  resumes a file that was cut off during the last hour instead of starting over.
- **Folders.** Dolphin's Share menu offers files only, so right-clicking folders shows **Send via Tailscale…**
  right in the context menu; it opens the same Share dialog. Folders are packed into a `.zip`, `.tar.gz`, `.tar.xz`
  or `.tar.zst` archive, which is sent under the folder's name. The dialog asks before it packs, and remembers the
  format you chose. A zip stores the files uncompressed and holds at most 4 GiB; pick a tar format for larger
  folders.
- **Device list.** The devices of your tailnet that accept files. The device you used last is listed first and
  preselected when it is online. Offline devices show when they were last seen and cannot be chosen. Online devices
  show how they are reached: **direct** (over your LAN or straight across the internet), **via peer relay**
  (through another device of your tailnet) or **via DERP relay**, which is usually much slower.
- **Clear errors.** The dialog tells you when Tailscale is not running, and how to fix it when your user may not
  send files. A failed share lists every file that was not sent, with the reason.

Only local files can be sent.

## Requirements

- KDE Plasma 6 / KDE Frameworks 6 and Qt 6.8 or newer (Debian 13, Ubuntu 26.04, Fedora, Arch Linux)
- [Tailscale](https://tailscale.com/download/linux) with Taildrop enabled for your tailnet
- Your user must be the Tailscale operator, so it can send files without root:

  ```sh
  sudo tailscale set --operator=$USER
  ```

  The Share dialog shows this command when it is needed. `tailscale file cp --targets` should list your devices.

## Installing

Download the package for your system and `SHA256SUMS` from the
[latest release](https://github.com/crevasse3523/plasma-taildrop/releases/latest), check the download, and
install it.

**Debian 13:** `plasma-taildrop_<version>_amd64.deb`

```sh
sha256sum -c --ignore-missing SHA256SUMS
sudo apt install ./plasma-taildrop_<version>_amd64.deb
```

**Ubuntu 26.04:** `plasma-taildrop_<version>~ubuntu26.04_amd64.deb`

```sh
sha256sum -c --ignore-missing SHA256SUMS
sudo apt install ./plasma-taildrop_<version>~ubuntu26.04_amd64.deb
```

**Fedora:** `plasma-taildrop-<version>-1.fc<NN>.x86_64.rpm`, built on the current Fedora release

```sh
sha256sum -c --ignore-missing SHA256SUMS
sudo dnf install ./plasma-taildrop-<version>-*.x86_64.rpm
```

**Arch Linux:** `plasma-taildrop-<version>-1-x86_64.pkg.tar.zst`

```sh
sha256sum -c --ignore-missing SHA256SUMS
sudo pacman -U ./plasma-taildrop-<version>-1-x86_64.pkg.tar.zst
```

Or build it yourself with `makepkg -si` in `packaging/`, which downloads the source of the release.

Share and the folder context menu pick the plugin up right away, with no restart. After an upgrade, an app that has
already used the plugin keeps the old version until it is quit completely and started again (`killall dolphin`).
Queueing and sending run in a separate helper, so their fixes apply at once.

Remove it with `sudo apt remove plasma-taildrop`, `sudo dnf remove plasma-taildrop` or `sudo pacman -R plasma-taildrop`.

## Building from source

Build dependencies:

| Distribution  | Command |
|---------------|---------|
| Debian 13 / Ubuntu 25.04+ | `sudo apt build-dep ./` in the source directory |
| Arch Linux    | `sudo pacman -S cmake extra-cmake-modules qt6-base qt6-declarative karchive kcoreaddons kdbusaddons ki18n kjobwidgets kio knotifications purpose` |
| Fedora        | `sudo dnf install cmake extra-cmake-modules gettext qt6-qtbase-devel qt6-qtdeclarative-devel kf6-karchive-devel kf6-kcoreaddons-devel kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kjobwidgets-devel kf6-kio-devel kf6-knotifications-devel kf6-purpose-devel` |

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

A first install needs no restart. When you reinstall over a version an app has already used, quit that app
completely (e.g. `killall dolphin`) and start it again.

To uninstall:

```sh
sudo cmake --build build --target uninstall
```

## Troubleshooting

**Send via Tailscale is not in the Share menu.** Check that the plugin is installed
(`dpkg -L plasma-taildrop | grep taildropplugin`). If you have just upgraded it, restart the app you share from.

**Send via Tailscale is not in the context menu of a folder.** Dolphin shows it only when all selected items are
folders, and its Share menu offers files only, so send the files and the folders of a mixed selection separately.
If it is still missing, open Dolphin's **Configure Dolphin… → Context Menu**, check **Send via Tailscale** and
restart Dolphin.

**"Tailscale only lets its operator send files."** Run `sudo tailscale set --operator=$USER`, then press Refresh
in the dialog.

**"Tailscale is not running."** Install Tailscale if it is missing, start it with
`sudo systemctl enable --now tailscaled` and log in with `sudo tailscale up`.

**A device is missing or cannot be chosen.** Only devices that accept files from you are listed: Taildrop must be on
for them, and devices of other users must be shared with you. A listed device that cannot be chosen is offline;
wake it up and press Refresh.

**A file failed with "the device stopped replying".** Nothing got through for 45 seconds. The device went to sleep or lost its
connection. Press Retry in the notification once it is back: a file that was cut off resumes where it stopped.

**Sending is slow.** Check how the device is reached in the dialog. **via peer relay** or **via DERP relay** means
Tailscale could not connect the two devices directly, often because of a strict firewall or NAT on one side. See
[Connection types](https://tailscale.com/kb/1257/connection-types) and `tailscale ping <device>`.

**Folders cannot be sent.** Press **Pack Folders** in the dialog and pick an archive format.

**tailscaled uses another socket.** Set `PLASMA_TAILDROP_SOCKET` to its path in the environment of your Plasma
session (for example with a script in `~/.config/plasma-workspace/env/` that exports it), then log in again. By default the plugin uses
`/run/tailscale/tailscaled.sock`.

**Logs.** The sending runs in `plasma-taildrop-send`; `journalctl --user -b | grep -i plasma-taildrop` shows
its messages.

## Development

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure    # unit tests
cmake --build build --target clang-format     # format the C++ code (needs clang-format)
./Messages.sh                                 # update po/ after changing translatable strings (needs gettext)
reuse lint                                    # check the license headers
```

Configuring the project installs a git pre-commit hook that checks formatting; `-DINSTALL_GIT_HOOKS=OFF` skips it.
The tests never talk to the real `tailscaled`: they run against a fake one on a temporary socket. See
[CONTRIBUTING.md](CONTRIBUTING.md) for more.

### Debian package

```sh
sudo apt build-dep ./
dpkg-buildpackage -us -uc -b                  # writes ../plasma-taildrop_<version>_amd64.deb
lintian --fail-on error,warning ../plasma-taildrop_*.changes
```

`debian/tests/smoke` is an autopkgtest that checks the installed package.

### Fedora package

```sh
sudo dnf builddep packaging/plasma-taildrop.spec
git archive --prefix=plasma-taildrop-X.Y.Z/ -o ~/rpmbuild/SOURCES/plasma-taildrop-X.Y.Z.tar.gz HEAD
rpmbuild -bb packaging/plasma-taildrop.spec     # writes ~/rpmbuild/RPMS/x86_64/plasma-taildrop-X.Y.Z-*.rpm
```

### Releasing

The release notes are the new entry in `debian/changelog`; there is no separate changelog file.

```sh
scripts/release.sh X.Y.Z        # sets the version, opens debian/changelog for the notes, commits and tags vX.Y.Z
git push --follow-tags
```

`release.sh` also sets the version in `packaging/plasma-taildrop.spec` and `packaging/PKGBUILD`. For a tag `v*`, CI
checks that the tag and all these files have the same version, builds and tests a `.deb` for Debian 13 and one for
Ubuntu 26.04, an `.rpm` for Fedora and a package for Arch Linux, and publishes them with `SHA256SUMS` and the
changelog entry as a GitHub release.

### How it works

| File | Role |
|------|------|
| `src/plugin/taildropplugin.cpp` | The Purpose plugin: checks the shared files and hands them over to `plasma-taildrop-send` |
| `src/fileitemaction/taildropfileitemaction.cpp` | The Dolphin context menu entry for folders: the entry of the Share menu, taken from a hidden one |
| `src/plugin/taildropplugin_config.qml` | The Share dialog page: what is sent, the archive format for folders, and the device picker |
| `src/qml/tailscaletargets.{h,cpp}` | QML model of the devices: file targets, connection paths, and whether this user may send |
| `src/qml/filesummary.{h,cpp}` | QML singleton: the dialog heading, problems with the shared files, and the remembered archive format |
| `src/helper/` | `plasma-taildrop-send`, one instance per session: queues every share, packs folders, uploads each file through tailscaled's LocalAPI, shows the progress in Plasma and the result as a notification, and remembers the device |
| `src/core/` | Static library: the tailscaled LocalAPI, the file-targets and status JSON, checking shared files, setting keys |
| `data/` | The desktop entry and the notification events of `plasma-taildrop-send`, and the AppStream metadata of the plugin |
| `autotests/` | Unit tests, with a fake `tailscaled` (`fakelocalapi.h`) and anonymized replies of a real one (`data/`) |
| `debian/` | The Debian package and its autopkgtest |
| `packaging/` | The Fedora (`.spec`) and Arch Linux (`PKGBUILD`) packages |
| `scripts/release.sh` | Sets the version, adds the changelog entry, commits and tags a release |

The last used device and the archive format are stored in `~/.config/plasma-taildrop.conf`.

## Translations

Translations live in `po/<language>/plasma-taildrop.po`. To add a language, run `./Messages.sh`, copy
`po/plasma-taildrop.pot` to `po/<language>/plasma-taildrop.po`, translate it, and open a pull request.
The `[pl]`-style keys in the `.json.in` files under `src/` and in `data/` are translated by hand.

## License

[GPL-2.0-or-later](LICENSE). The AppStream metadata and the test data are CC0-1.0. The project follows the
[REUSE](https://reuse.software) specification.
