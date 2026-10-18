#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$source_dir/../../build/linux"}
kind=${2:-deb}
build_dir=$(realpath "$build_dir")

case "$kind" in
    deb)
        (cd "$build_dir" && cpack -G DEB)
        ;;
    appimage)
        : "${LINUXDEPLOY:?Set LINUXDEPLOY to a linuxdeploy AppImage executable}"
        : "${LINUXDEPLOY_QT_PLUGIN:?Set LINUXDEPLOY_QT_PLUGIN to the Qt plugin AppImage}"
        if [[ $(uname -m) != x86_64 ]]; then
            echo 'AppImage packaging currently supports x86_64 only.' >&2
            exit 2
        fi
        appdir="$build_dir/AppDir"
        if [[ -e "$appdir" ]]; then
            echo "Refusing to overwrite existing AppDir: $appdir" >&2
            exit 2
        fi
        DESTDIR="$appdir" cmake --install "$build_dir" --prefix /usr
        # Extract supplied build tools into an isolated temporary directory;
        # avoids depending on FUSE inside CI and preserves the original tools.
        tools_dir=$(mktemp -d "$build_dir/appimage-tools.XXXXXX")
        trap 'rm -rf -- "$tools_dir"' EXIT
        mkdir "$tools_dir/deploy" "$tools_dir/qt"
        (cd "$tools_dir/deploy" && "$LINUXDEPLOY" --appimage-extract >/dev/null)
        (cd "$tools_dir/qt" && "$LINUXDEPLOY_QT_PLUGIN" --appimage-extract >/dev/null)
        ln -s "$tools_dir/qt/squashfs-root/AppRun" "$tools_dir/linuxdeploy-plugin-qt"
        export PATH="$tools_dir:$PATH"
        export QMAKE=${QMAKE:-/usr/bin/qmake6}
        export EXTRA_QT_MODULES=svg
        export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so
        export ARCH=x86_64
        export APPIMAGE_EXTRACT_AND_RUN=1
        export OUTPUT="$build_dir/Snow_Shot_Linux_Preview-x86_64.AppImage"
        "$tools_dir/deploy/squashfs-root/AppRun" --appdir "$appdir" --plugin qt --output appimage
        ;;
    *) echo "Usage: $0 BUILD_DIR {deb|appimage}" >&2; exit 2 ;;
esac
