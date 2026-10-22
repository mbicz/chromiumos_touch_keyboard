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
`toggle-application-view`). F10 has no visible effect when only one input
source or layout is configured. Plain F1–F12 remain normal function keys.
