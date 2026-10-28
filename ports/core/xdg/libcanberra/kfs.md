## About
[libcanberra]{.idx} plays event sounds by name from the XDG sound theme specification: an
application asks for `message-new-instant` or `bell-window-system`, and libcanberra finds the file
in the installed theme, here [sound-theme-freedesktop](#port-sound-theme-freedesktop), and plays it.
The port installs the library, `libcanberra-gtk3` and the GTK 3 module that plays feedback sounds
for widgets, and `canberra-gtk-play`. Ports that use it include KDE's KNotifications framework,
Dino, Brasero and GNOME Disks.

## Flags
- `--enable-alsa` — with `--enable-pulse`, `--enable-gstreamer` and `--enable-null`: the output
  backends, each loaded as a module. [PipeWire](#port-pipewire) answers both the ALSA and the
  PulseAudio one; `null` plays nothing.
- `--disable-oss` — no OSS backend, which has no device in KDOS.
- `--enable-tdb` — the sound lookup cache, kept in a [tdb](#port-tdb) database; named, a missing
  tdb stops configure. The last line of `build.sh` also checks that `libcanberra.so` has `libtdb`
  among its needed libraries.
- `--enable-gtk3` — with `--disable-gtk`: build the GTK 3 library and module, and not the GTK 2
  ones.
- `--enable-udev` — build `canberra-boot`, the start-up sound tool, which is the only part of
  libcanberra that uses udev; `build.sh` deletes it, since nothing in KDOS starts it.
- `--with-systemdsystemunitdir=no` — install no systemd units; there is no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--disable-lynx` — with `--disable-gtk-doc`: do not regenerate the README with lynx or the API
  reference with gtk-doc.

## Notes
`dont-assume-all-GdkDisplays-are-GdkX11Displays.patch` is upstream's (it carries its header). The
GTK 3 module and `libcanberra-gtk3` read window properties through Xlib; the patch returns early
on a display that is not an X11 one, so a GTK 3 application on Wayland plays its sounds instead of
crashing in Xlib.

`make install` runs with `-j1`: libtool relinks each backend module against the library as it
installs it, and a parallel install can relink a module before the library is in place.

The gtk-doc HTML reference ships prebuilt in the tarball and installs whatever `--disable-gtk-doc`
says, so `build.sh` removes it. It also removes the GNOME session autostart and shutdown files, the
GDM autostart file and the `gnome-settings-daemon-3.0` entry that asks that daemon to load the GTK
module, which name programs KDOS does not have.
