## About

[gLabels]{.idx} is a GTK 3 program for designing and printing labels, business cards and envelopes
on standard label sheets, with text, images, barcodes and mail merge from a CSV file. Its program
is `glabels-3`.

## Flags

- `CFLAGS="$CFLAGS -fcommon -Wno-error=incompatible-pointer-types"` — `-fcommon` because two of
  gLabels' headers define a global variable instead of declaring it, so every object that includes
  one carries its own definition; GCC's default, `-fno-common`, makes the link fail on the
  duplicates, and `-fcommon` merges them. `-Wno-error=incompatible-pointer-types` turns the
  pointer conversions between incompatible types, which GCC rejects as errors, back into warnings.
- `--disable-gtk-doc` — Build no gtk-doc API reference for libglabels, the library gLabels installs with the program.
- `--disable-schemas-compile` — Do not run `glib-compile-schemas` at install time; kpkg rebuilds
  the shared GSettings schema cache after an install as one of its
  [shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).
- `--without-libzint` — No Zint barcode backend. gLabels' backend calls `ZBarcode_Render`, which
  the library of [zint](#port-zint) does not have, so configure would not find it and the backend
  could not build against the port.
- `--without-libbarcode`, `--without-libiec16022`, `--without-libebook` — No GNU Barcode, no
  libiec16022 DataMatrix backend and no Evolution address book for mail merge. None of the three is
  a port.
- `--with-libqrencode` — QR codes through [qrencode](#port-qrencode). The other barcode types come
  from gLabels' built-in encoders.

## Notes

A `.glabels` file is gzipped XML, and libxml2 decompresses a file only when the parser is asked
to; without `xml-unzip.patch` every saved label fails to open.

Only the English help under `/usr/share/help/C` is kept.

The program has no GtkApplication and calls no `set_prgname`, so its Wayland app id is the program
name, `glabels-3`. `build.sh` replaces upstream's desktop entry, `glabels-3.0.desktop`, named
"gLabels Label Designer 3" and with no `StartupWMClass`, with `glabels-3.desktop`, which names that
id in `StartupWMClass`.
