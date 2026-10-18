"""Real X11 grab, selection, PNG save and single-instance capture smoke test.

Run under xvfb-run. Only the new Linux frontend is exercised.
"""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import time


def xdo(*args):
    return subprocess.run(["xdotool", *map(str, args)], check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.strip()


def wait_for(probe):
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        try:
            value = probe()
            if value:
                return value
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
        time.sleep(0.1)
    raise AssertionError("Timed out waiting for X11 smoke test state")


binary = Path(sys.argv[1]).resolve()
env = dict(os.environ, LANG="en_US.UTF-8", LC_ALL="en_US.UTF-8", QT_QPA_PLATFORM="xcb")
with tempfile.TemporaryDirectory(prefix="snow-linux-test-") as directory:
    # Isolate single-instance IPC and settings from the user's running app.
    env.update(XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=directory)
    app = subprocess.Popen([str(binary), "--capture"], env=env)
    try:
        def full_screen_window():
            for window in xdo("search", "--onlyvisible", "--pid", app.pid).splitlines():
                geometry = xdo("getwindowgeometry", "--shell", window)
                values = dict(line.split("=", 1) for line in geometry.splitlines() if "=" in line)
                if int(values.get("WIDTH", 0)) >= 1000:
                    return window
            return None

        selection = wait_for(full_screen_window)
        xdo("windowfocus", selection)
        xdo("mousemove", 100, 100, "mousedown", 1, "mousemove", 300, 240, "mouseup", 1)
        editor = wait_for(lambda: xdo("search", "--onlyvisible", "--name",
                                     "Snow Shot Linux Preview.*Editor").splitlines()[0])
        xdo("windowfocus", editor)
        xdo("key", "ctrl+s")
        dialog = wait_for(lambda: xdo("search", "--onlyvisible", "--name", "Save screenshot").splitlines()[0])
        xdo("windowfocus", dialog)
        path = Path(directory) / "capture.png"
        xdo("key", "ctrl+a")
        xdo("type", "--clearmodifiers", "--delay", 1, str(path))
        xdo("key", "Return")
        wait_for(lambda: path.is_file() and path.stat().st_size > 24)
        data = path.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", "saved file must be PNG"
        assert struct.unpack(">II", data[16:24]) == (200, 140), "selected pixel dimensions"
        subprocess.run([str(binary), "--capture"], env=env, check=True, timeout=6)
        wait_for(full_screen_window)
        xdo("key", "Escape")
        wait_for(lambda: xdo("search", "--onlyvisible", "--name", "Snow Shot Linux Preview.*Editor"))
        assert app.poll() is None, "primary instance remains alive"
        print("X11 capture, PNG save and single-instance smoke test passed.")
    finally:
        app.terminate()
        try:
            app.wait(timeout=5)
        except subprocess.TimeoutExpired:
            app.kill()
            app.wait()
