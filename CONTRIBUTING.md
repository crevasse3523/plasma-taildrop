# Contributing

Bug reports, translations and pull requests are welcome. For a larger change, open an issue first so we can agree
on the approach.

## Setting up

Install the build dependencies listed in the [README](README.md#building-from-source), then:

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

To try the plugin, configure with `-DCMAKE_INSTALL_PREFIX=/usr`, install it (`sudo cmake --install build`). An app
that has already loaded an older build keeps it, so quit it completely before trying again (`killall dolphin`).

## Before you open a pull request

- `ctest` passes. New behaviour comes with a test in `autotests/`.
- The C++ code is formatted: `cmake --build build --target clang-format`. The pre-commit hook installed at configure
  time checks this.
- The Share dialog is clean in qmllint:

  ```sh
  /usr/lib/qt6/bin/qmllint --max-warnings 0 --bare -I build/bin -I /usr/lib/x86_64-linux-gnu/qt6/qml \
      src/plugin/taildropplugin_config.qml
  ```

- Every new file has an `SPDX-FileCopyrightText` and `SPDX-License-Identifier` header, or an entry in
  `REUSE.toml`; `reuse lint` passes.
- After changing translatable strings, run `./Messages.sh` and update `po/pl/plasma-taildrop.po` if you can.

CI builds and tests on Arch Linux and Debian 13. On Debian it also checks the formatting, runs qmllint, checks that
`po/` has every string (translations may be missing) and builds and lints the Debian package. `reuse lint` runs on
its own.

## Guidelines

- Follow the existing style: KDE coding style, Qt and KDE Frameworks APIs, short comments that say why.
- Keep the plugin light: it runs inside the app that shares, so all long work belongs in `plasma-taildrop-send`.
- Talk to tailscaled only through its LocalAPI (`src/core/localapi.h`), never by running the `tailscale` command.
- Tests must never contact the real tailscaled or send files anywhere: use the fake in `autotests/fakelocalapi.h`.
- Commit messages: a short summary in the imperative, then what changed and why.

## License

By contributing you agree that your contribution is licensed under GPL-2.0-or-later, or CC0-1.0 for metadata and
test data, like the rest of the project.
