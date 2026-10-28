## About
[Brasero]{.idx} is GNOME's disc burner, a GTK 3 application: it writes data, audio and video CDs
and DVDs, burns image files, and copies discs. It does no burning itself. Each way of writing a
disc is a plugin under `/usr/lib/brasero3/plugins`, which either links a burning library or runs a
burning program, and Brasero picks among the plugins that are present. [K3b](#port-k3b) and
[xfburn](#port-xfburn) are the tree's other burners.

## Flags
- `--enable-libburnia` — the plugins built on libburnia's libraries: [libburn](#port-libburn)
  writes the disc and [libisofs](#port-libisofs) builds the ISO 9660 image, both in process.
- `--enable-cdrdao` — the plugin that runs [cdrdao](#port-cdrdao), for writing and copying a CD
  disc-at-once.
- `--enable-growisofs` — the plugins that run `growisofs` and `dvd+rw-format` from
  [dvd+rw-tools](#port-dvdxrw-tools), for DVDs.
- `--disable-cdrtools` — with `--disable-cdrkit`: no plugins for `cdrecord`, `cdda2wav`, `readcd` and
  `mkisofs` from cdrtools, or for `wodim`, `readom` and `genisoimage` from cdrkit. Neither suite
  is a port. Upstream's default builds both.
- `--enable-playlist` — the playlist pane, which reads playlists with
  [totem-pl-parser](#port-totem-pl-parser). Upstream's default: configure builds without the pane,
  and without an error, when that library is missing, with the switch or without it.
- `--enable-preview` — the preview pane, which plays a file through GStreamer's base plugins.
  Upstream's default; without the plugins configure leaves the pane out without an error, as for
  the playlist.
- `--enable-inotify` — watch the files and folders of a data project through inotify, so the
  project follows changes made to them on disk. Upstream's default, named.
- `--disable-search` — no search pane. It needs Tracker, which is not a port; upstream's default
  is `auto`.
- `--disable-nautilus` — no Nautilus extension. Nautilus is not a port, and upstream's default
  builds the extension.
- `--enable-introspection=no` — no GObject introspection data for `libbrasero-burn` and
  `libbrasero-media`, the typelibs that would let another language call them.
- `--disable-caches` — do not run `update-mime-database` and `update-desktop-database` from
  `make install`. Upstream's rules for both also skip themselves when the staging directory has no
  `freedesktop.org.xml` or `defaults.list`; the switch makes that a setting of the recipe rather
  than a test of what the staging directory holds, and matters most for `update-desktop-database`,
  which upstream runs on the live system's database, not the staged one. Upstream's default runs
  them. The switch does not reach `gtk-update-icon-cache`, which upstream's icon rule skips on its
  own whenever `DESTDIR` is set.

## Patches
- `gcc14.patch` — upstream's commit for GCC 14, which treats `-Wincompatible-pointer-types` as an error. It casts the plugin list passed to `gst_install_plugins_async()` to the `const gchar *const *` the function takes, in `brasero-metadata.c` and `brasero-pk.c`, and in `brasero-drive-properties.c` drops the local variable that stored `g_value_get_object()` into a `BraseroBurnSession *`.

## Notes
`build.sh` writes its own `brasero.desktop` over upstream's. Every `Exec` line, the main one and
the three actions (burn an image, copy a disc, start an audio project), runs Brasero with
`GDK_BACKEND=x11`, so it runs under Xwayland: its dialogs and its video preview take X11 window
ids.

The translated help under `/usr/share/help` is removed; only the C original ships.
