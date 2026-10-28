## About
Android File Transfer reaches a phone's storage over MTP, the protocol an Android phone offers on
USB. It speaks MTP itself through the kernel's usbdevfs interface, without [libmtp](#port-libmtp),
and gives three ways in: `aft-mtp-mount ~/Phone` mounts the storage as a FUSE directory any file
manager reads (`fusermount3 -u ~/Phone` takes it away), the `android-file-transfer` window browses
and copies with no mount, and `aft-mtp-cli` is an interactive shell. libmtp's own `mtp-*` tools
address files only by numeric id.

## Flags
- `-DBUILD_QT_UI=ON` — build the `android-file-transfer` window. It is upstream's default, named.
- `-DDESIRED_QT_VERSION=6` — look for Qt 6 alone. Left unset, the build takes Qt 6, Qt 5 or Qt 4,
  in that order, and with none of them builds no window and prints only a warning; the check after
  the install turns that into a failure.
- `-DBUILD_FUSE=ON` — build `aft-mtp-mount` against [fuse](#port-fuse). It is upstream's default.
- `-DBUILD_MTPZ=OFF` — no MTPZ, the authentication handshake of Microsoft's Zune players. Upstream
  builds it against OpenSSL and fetches the key data it needs at run time from a URL compiled into
  the program.
- `-DBUILD_PYTHON=OFF` — no `aftl` Python module.
- `-DBUILD_TAGLIB=OFF` — no TagLib, which upstream uses to read an audio file's tags into its MTP
  metadata on upload.
- `-DBUILD_SHARED_LIB=OFF` — link the MTP library, `libmtp-ng-static`, into the three programs
  rather than building it shared. It is upstream's default; `build.sh` deletes the static archive
  from the package, since nothing else links it.

## Notes
[libmtp](#port-libmtp) is in `depends` for its udev rules, `69-libmtp.rules`, which give the
`dialout` group access to a phone's USB device node; this program does not link it. libmagic,
from [file](#port-file), gives an uploaded file its MIME type.

The desktop entry is KDOS's own. Upstream's has no `StartupWMClass`; the program sets the
organisation domain `whoozle.github.io` and no desktop file name, so Qt makes its Wayland app id
the reversed domain followed by the executable's name, `io.github.whoozle.android-file-transfer`,
and the entry names that. The application name the program also sets, `aft-linux-qt`, plays no
part in it.
