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
The daemon consumes `F10 / 68` locally to cycle the keyboard backlight;
other codes are emitted as ordinary input events.
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
source or layout is configured. Plain F1–F9 and F11–F12 remain normal function
keys.

Plain F10 cycles the Halo keyboard LED **off → half → full → off**, once per
press (no stepping while held). It is consumed completely, including its
release; Fn+F10 still emits `KEY_KEYBOARD / 374` for input-source switching,
even if Fn is released first. Both supplied pc104 and pc105 layouts retain
their F10/68 and Fn KEYBOARD/374 numeric mappings.

The existing root daemon uses only the known Yoga Book standard LED
interfaces under `/sys/class/leds`: Android `yogabook::kbd_backlight` is
preferred; if absent, Windows `ybwmi::kbd_backlight` is selected when present.
It never searches unrelated keyboard LEDs. It reads the selected interface's
`brightness` and `max_brightness` on each press: a maximum of 255 gives levels
0, 128, 255.
An intermediate brightness advances to the next higher level; full wraps to
off, so startup and external changes are respected. A missing, invalid or
unwritable LED produces an error in `journalctl -u touch-keyboard-handler`
without stopping typing or falling back to emitting F10. A subsequent new
press can retry after the interface is restored. No desktop binding or
additional service is needed.

The deterministic hinge, native multitouch-gate and backlight regression tests run with
`ctest --test-dir build --output-on-failure` after the CMake build, and through
the Debian package's normal `dh_auto_test` step.
