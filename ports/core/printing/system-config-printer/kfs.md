## About

[system-config-printer]{.idx} is the GTK 3 printer-configuration program for
[CUPS](#port-cups), written in Python: it finds printers, adds and removes print queues, picks a
driver or PPD for each, and changes a queue's options. It also installs the `cupshelpers` Python
module it is built on. The desktop entry calls it Print Settings.

## Flags

- `--without-udev-rules` — Do not install the USB auto-configuration helper, udev rules that add a
  print queue when a USB printer is plugged in. It is upstream's default. Queues are set up through
  cups-browsed and this program.
- `--without-systemdsystemunitdir` — Install no systemd unit. Without the switch configure asks
  pkg-config for systemd's unit directory; KDOS has no systemd (see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--with-xmlto` — Build the manual pages from DocBook with `xmlto`, through
  [docbook-xml](#port-docbook-xml) and [docbook-xsl](#port-docbook-xsl).

## Patches

- `makefile-am-fix-setup-py-install.patch` — makes the Python module's install rule in `Makefile.am` call `setup.py install --root=$(DESTDIR) --prefix=$(prefix)` instead of `--prefix=$(DESTDIR)$(prefix)`, so the module is staged under `DESTDIR` while its installed paths name `/usr`, not the staging directory.

## Notes

The source is configured from `configure.ac`: `gettext-1.0.patch` changes it, and `autoreconf -fi`
regenerates the build files after both patches.

`build.sh` removes `/etc/xdg/autostart/print-applet.desktop`. The print applet is a
`GtkStatusIcon`, which draws only on an X11 tray; started in every session, it would run with no
icon to show.

`build.sh` replaces upstream's desktop entry. The program calls
`GLib.set_prgname("system-config-printer")`, which becomes its Wayland app id, and the entry names
it in `StartupWMClass`. `Icon=printer` is the printer icon of the KDOS icon theme.
