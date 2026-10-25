## About

android-tools is the Android platform tools built outside AOSP: `adb`, for a phone's shell, logs,
files and app installs, and `fastboot`, for flashing, with the image utilities beside them
(`img2simg`, `simg2img`, `lpmake`, `lpunpack`, `mkbootimg`, `unpack_bootimg`, `avbtool`,
`mke2fs.android`, `make_f2fs` and others). The release tarball carries the AOSP
sources it builds under `vendor/`. The iPhone's side of the same job is usbmuxd and ifuse.

## Flags

- `-Wl,--copy-dt-needed-entries` — Let the linker follow the `DT_NEEDED` entries of the shared
  libraries on the link line to resolve symbols. The protobuf code adb generates calls into abseil,
  and the build's targets do not name every absl library it needs; libprotobuf's own `DT_NEEDED`
  entries do, and with this flag the link takes them from there, where it can otherwise stop on
  unresolved `absl::` symbols.
- `-DANDROID_TOOLS_PATCH_VENDOR=OFF` — Skip the step that applies `patches/` to the vendored AOSP
  trees with `git submodule` and `git am`. The release has no `patches/` directory and the build
  tree is no git repository; the switch keeps the step off whatever the tarball holds.
- `-DANDROID_TOOLS_USE_BUNDLED_FMT=OFF` — With `-DANDROID_TOOLS_USE_BUNDLED_LIBUSB=OFF`, link the
  [fmt](#port-fmt) and [libusb](#port-libusb) ports instead of the copies under `vendor/`, so a fix
  to either reaches adb and fastboot. Both are upstream's defaults, named.

## Notes

`gtest` is a build dependency only: headers of libbase and fastboot include `gtest_prod.h`.

`build.sh` writes a udev rule, `/usr/lib/udev/rules.d/70-android-tools.rules`, that gives the
`dialout` group read and write access to a USB device offering interface class `ff`, subclass `42`,
protocol `01` (adb) or `03` (fastboot). Those are the numbers adb and fastboot themselves look for
(`ADB_CLASS`, `ADB_SUBCLASS` and `ADB_PROTOCOL` in adb, `match_fastboot_with_serial()` in fastboot),
so the rule matches a phone by its interface and needs no table of vendor IDs. Without
the rule the device node is root's alone and `adb devices` lists nothing for the console user, who
is in `dialout`.
