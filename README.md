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

## libinput quirk

The virtual touchpad must not be suspended when the virtual tablet-mode
switch is pulsed ON (that switch exists only to keep GNOME touch mode /
auto-rotation alive).  The package installs a libinput quirk file at
`/usr/share/libinput/61-yogabook.quirks` setting `ModelTabletModeNoSuspend`
for the `virtual-touchpad` device.  Without it, libinput pairs the touchpad
with the switch and drops all trackpad events 5 seconds after every
handler (re)start.
