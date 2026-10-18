# Snow Shot Linux Preview (Kubuntu / X11)

This is a **new, independent Qt 6 Linux frontend in the Snow Shot fork**, version
0.1.0. It is an experimental first step toward Linux support, **not a port of the
complete upstream Snow Shot UI, Rust capture stack, drawing engine or feature set**.
The existing Windows/macOS applications and build presets are unchanged. Source
in this directory uses the enclosing Snow Shot GPL-3.0-or-later license.

Target: **Kubuntu 24.04, Plasma X11, x86_64**. Qt 6.4 from Ubuntu is sufficient;
the upstream workspace's Qt 6.12 / Rust / vcpkg environment is not required.

## Available functionality

- Region selection across multiple monitors; negative monitor coordinates and
  different screen pixel densities are composed into a common output scale.
- Pen, rectangle, ellipse, arrow, line color/width, undo.
- PNG save, image clipboard, independent always-on-top image pins.
- Capture delay, tray menu, single-instance capture commands, English,
  Traditional Chinese and Simplified Chinese UI chosen from the desktop locale.
- Application menu entry and capture/tray desktop actions.

Wayland is rejected with an explicit message. OCR, scrolling capture, recording,
text annotations, automatic window detection, upstream drawing tools, upstream
settings/history/MCP, automatic updates and HDR capture are **not implemented**.
Mixed-DPI monitor geometry is covered by synthetic tests; actual Plasma fractional
scaling, multi-GPU monitors and clipboard persistence after application exit still
need testing on hardware. Keep the app running when pasting a copied image.

## Build and test on Kubuntu 24.04

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-tools-dev qt6-l10n-tools libqt6svg6 python3 dpkg-dev
cmake -S snow_shot/linux -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/linux --parallel 2
ctest --test-dir build/linux -R '^snow-linux-(image|widget)-tests$' --output-on-failure
./build/linux/snow-shot-linux
```

For a real X11 capture/save/single-instance smoke test:

```sh
sudo apt install xvfb xdotool
xvfb-run -a python3 snow_shot/linux/tests/x11_smoke.py build/linux/snow-shot-linux
```

Translation catalogs are independent of the upstream feature fragments. After
changing UI strings, extract this target with `snow-shot-linux_lupdate`, fill all
three catalogs, and run `python3 snow_shot/linux/tests/check_translations.py`.
Qt 6.4 does not support `lrelease -fail-on-unfinished`, so the configure-time
validator enforces complete catalogs instead.

## deb / apt install

```sh
bash snow_shot/linux/packaging/package.sh build/linux deb
sudo apt install ./build/linux/snow-shot-linux_0.1.0_amd64.deb
snow-shot-linux --capture
sudo apt remove snow-shot-linux
```

This is a local `.deb` installed with apt, **not an online apt repository**.
Dependency resolution uses the normal Ubuntu repositories. A signed apt repository
for `apt install snow-shot-linux` and automatic upgrades is a separate deployment
step and is not provisioned by this preview.

## AppImage

The `Snow Shot Linux preview` workflow builds both formats on Ubuntu 24.04,
runs focused tests, and retains the downloadable artifacts for **3 days**.
It uses pinned AppImage tooling asset IDs with SHA-256 verification. It does not
publish a release or modify any apt sources.

For a local AppImage build, download linuxdeploy and its Qt plugin from their
official release pages, verify the hashes, then provide their absolute paths:

```sh
export LINUXDEPLOY=/absolute/path/linuxdeploy-x86_64.AppImage
export LINUXDEPLOY_QT_PLUGIN=/absolute/path/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$LINUXDEPLOY" "$LINUXDEPLOY_QT_PLUGIN"
bash snow_shot/linux/packaging/package.sh build/linux appimage
chmod +x build/linux/Snow_Shot_Linux_Preview-x86_64.AppImage
./build/linux/Snow_Shot_Linux_Preview-x86_64.AppImage
```

Run with `APPIMAGE_EXTRACT_AND_RUN=1` if FUSE is unavailable. The AppImage is
built on Ubuntu 24.04 and targets Kubuntu 24.04 or newer; it is not intended to
run on older glibc distributions. Remove an existing `build/linux/AppDir` yourself
before repeating packaging, or use a fresh build directory.

## KDE shortcut and startup

In **System Settings → Shortcuts → Custom Shortcuts**, assign a shortcut such as
`Meta+Shift+S` to `snow-shot-linux --capture` (use the absolute AppImage path when
using AppImage). An existing instance receives the command. To avoid replacing
Spectacle's bindings, Print Screen is not claimed automatically.

To start with KDE, add `snow-shot-linux --tray` to **System Settings → Startup
and Shutdown → Autostart**. This is optional; no login settings are changed by
installation. To exit, use the tray menu's Quit action.
