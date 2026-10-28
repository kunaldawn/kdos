## About

[Xfburn]{.idx} is Xfce's GTK 3 disc burner: data and audio CDs and DVDs, and ISO images, written
directly through [libburn](#port-libburn) and composed with [libisofs](#port-libisofs). It runs no
external writer program, so it needs nothing at run time beyond the libraries it links.

## Flags

- `-Dgudev=enabled` — List optical drives and notice a disc change through udev, by
  [libgudev](#port-libgudev). The feature is `auto` upstream, which drops it when the library is
  missing; `enabled` makes a missing one a configuration error.
- `-Dgstreamer=enabled` — Decode the files of an audio composition to CD audio through
  GStreamer's `decodebin`, so any format the installed plugins read (MP3, FLAC, Ogg Vorbis among
  them) can go onto an audio CD; required in the same way.

## Notes

The manual page is built by `xsltproc` from DocBook with `--nonet`, so the stylesheet URL has to
resolve through the catalog of [docbook-xsl](#port-docbook-xsl), which is why that port is a
dependency.

`build.sh` removes `/usr/share/Thunar`, the "Send To" entry for a file manager that is not in the
tree.

Upstream installs its icon, `stock_xfburn`, under hicolor's `stock/media` context, which the
panel's icon lookup does not read. `build.sh` rasterises the same SVG into `apps/xfburn.png` at
48, 64 and 128 pixels, and the desktop entry it writes names that icon. The entry replaces
upstream's: GTK 3 takes the Wayland app id from the program name, so `StartupWMClass=xfburn`, and
the name is qualified, "Disc Burner (Xfburn)", as [Brasero](#port-brasero)'s is "Disc Burner
(Brasero)", so the menu tells the two GTK burners apart.
