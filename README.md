# Touch Keyboard

This is fork of ChromiumOS touch keyboard driver found at
https://chromium.googlesource.com/chromiumos/platform2/+/19effa94d56d31397a55292771c8ea196f419f1e/touch_keyboard/

To get some details about the project, check the [README.upstream.md](README.upstream.md) file.

## Build and install

```
apt install build-essential cmake
mkdir build
cd build
cmake ../
make
sudo make install
```

Or just run the `dpkg-buildpackage -b --no-sign` command to build .deb package.

## Configuration
To create custom keyboard layout, edit the file layout.csv and place it as /etc/touch_keyboard/layout.csv.

The CSV is the key-to-action mapping; no daemon rebuild is needed. `code` is
the normal Linux input keycode, and `code_fn` is selected while Fn is held.
`name`/`name_fn` are descriptive labels, not keycode lookup expressions.
Edit the numeric code as well as its label, then restart
`touch-keyboard-handler.service`. Keep the correct US pc104 or International
pc105 geometry; editing hotkeys does not require changing key rectangles.

Fn is held only by a live, unrejected touch that started on its key. It
participates immediately, before the 50 ms confirmation checks. Sliding off
Fn, lifting the finger, or disabling the keyboard releases the modifier.
Once the pressure or contact-size checks reject a touch, it no longer holds
Fn; this does not remap key events already queued while Fn was provisional.

The hinge gate releases committed keys and drops pending actions while
disabled, but continues tracking raw per-slot ABS deltas. Reopening suppresses
all contacts already held until their next tracking ID, even if they move.
New contacts retain the slot's cached coordinates and contact size, so typing
at exactly the same position works without movement to refresh those axes.
At startup the daemon also seeds those native slot caches with evdev ioctls,
without promoting already-held contacts. This allows a service restart to
accept the next tracking-ID-only press at unchanged coordinates. Unsupported
pressure remains absent, so contact-size validation still applies. Query
failures warn and leave the affected axes awaiting real source updates.

The icon mappings in both layouts are:

| Fn key | Linux name / code | Intended action |
|---|---|---|
| F4 | MICMUTE / 248 | Microphone mute |
| F7 | SEARCH / 217 | Desktop search, not accessibility zoom |
| F8 | RFKILL / 247 | Airplane-mode toggle |
| F9 | ALL_APPLICATIONS / 204 | Application grid (`XF86LaunchB`) |
| F10 | KEYBOARD / 374 | Input-method/layout switch (`XF86Keyboard`) |
| F11 | SCALE / 120 | Desktop/workspace overview (`XF86LaunchA`) |

These are ordinary key events; the desktop environment decides what they
do. Desktops commonly handle the microphone, search, radio and input-source
keys out of the box. `XF86LaunchA`/`XF86LaunchB` may need binding to the
desktop's overview and application-grid actions (in GNOME:
`org.gnome.shell.keybindings toggle-overview` and
`toggle-application-view`). Fn+F10 has no visible effect when only one input
source or layout is configured. Plain F1–F12 remain normal function keys.

The deterministic hinge and native multitouch-gate regression tests run with
`ctest --test-dir build --output-on-failure` after the CMake build, and through
the Debian package's normal `dh_auto_test` step.
