## About

libgpod reads and writes the music database of an [iPod]{.idx}: the classic, nano and shuffle models
that hold their library in an iTunesDB. rhythmbox and strawberry sync to an iPod through it. It also
installs the udev callouts that read an iPod's identity when it is plugged in.

## Flags

- `LIBGPOD_CFLAGS` — With `LIBGPOD_LIBS`, hand configure the compile and link flags of the core
  module set from `pkg-config`, for `glib-2.0`, `gobject-2.0`, `gmodule-2.0`, `sqlite3` and
  `libplist-2.0`. The configure script asks pkg-config for a module named `libplist`, a name
  libplist 2 does not install (it is `libplist-2.0`), and stops; with both variables set, the
  pkg-config check is skipped and the flags are taken as given.
- `--disable-more-warnings` — Add no warning flags and no `-Werror`. The configure script turns them
  on by default only when `autogen.sh` is present, which it is not in the release; the switch makes
  that explicit.
- `--enable-udev` — With `--with-udev-dir=/usr/lib/udev`, install the `ipod-set-info` and
  `iphone-set-info` callouts and their rule under `/usr/lib/udev`, where eudev reads them. When an
  iPod is plugged in, the callout writes its `SysInfoExtended` file. libgpod reads the iPod's
  FireWire ID from that file, and from the iPod Classic and the video nano on, an iPod shows a
  database written without that ID as empty.
- `--with-libimobiledevice` — Support iPhone and iPod touch devices through libimobiledevice.
- `--enable-libxml` — Parse `SysInfoExtended` with libxml2.
- `--enable-gdk-pixbuf` — Write cover art into the ArtworkDB, which needs gdk-pixbuf.
- `--disable-pygobject` — With `--without-python`, build no Python binding. The binding is generated
  with SWIG, and its GdkPixbuf support asks for `pygobject-2.0`, PyGObject 2's module, which the
  tree's [python3-gobject](#port-python3-gobject) does not provide.
- `--without-hal` — Build no HAL callouts. HAL is not a port; udev does the job.
- `--without-mono` — Build no .NET binding.
- `DATADIRNAME=share` — Install the message catalogues under `/usr/share/locale`. The configure
  script picks `share` only when GNU gettext's internal `_nl_msg_cat_cntr` links, and `lib`
  otherwise.

## Patches

- `gcc14.patch` — casts the result of `g_object_ref()` to `GdkPixbuf *` in two places in `src/ithumb-writer.c`, and gives `get_info_from_usb()` in `tools/generic-callout.c` a prototype with typed parameters instead of a K&R parameter list with no types; current GCC rejects both by default.
- `pkgconfig-requires-private.patch` — moves glib, gobject, gdk-pixbuf and libimobiledevice from `Requires:` to `Requires.private:` in `libgpod-1.0.pc`, so a program linking libgpod is not linked against all of them as well.

## Notes

libusb, which the callout uses to read `SysInfoExtended` from a nano 5G, has no switch: configure
finds it or drops that read, so libusb is in `depends`. sg3_utils, the SCSI path for older models,
is not a port, so that path is absent.

The install writes `libgpod-sharp.pc` even without the .NET binding, and makes the callouts' mount
parent, `/tmp`, which the base file system already owns; `build.sh` removes both.
