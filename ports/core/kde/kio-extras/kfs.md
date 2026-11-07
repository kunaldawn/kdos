## About

kio-extras is the set of KIO workers, thumbnailers and settings modules every KDE application
opens files through beyond the local disk: `smb:/` for Windows and Samba shares, `sftp:/` and
`fish:/` over SSH, `mtp:/` for phones and cameras, `afc:/` for iOS devices, `man:/` and `info:/`,
the archive and `recentlyused:/` workers, and the thumbnail worker with its plugins for pictures,
audio covers, EXR, cursors, DjVu, comics and text. It also builds the Trash, Proxy, Connection
Preferences and Web Search Keywords pages of System Settings.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image
  is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Builds the handbooks of the workers, upstream's default, named;
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` makes [kdoctools](#port-kdoctools) a requirement,
  so a missing one stops the configure.
- `-D BUILD_KCMS=ON` — Builds the four settings modules (`kcm_trash`, `kcm_proxy`, `kcm_netpref`,
  `kcm_webshortcuts`), which System Settings shows under Network and Files; it requires
  [kcmutils](#port-kcmutils). Upstream's default, named.
- `-D BUILD_ACTIVITIES=ON` — Builds the `activities:/` and `recentlyused:/` workers and the
  "Forget" file-item action, on [plasma-activities](#port-plasma-activities) and
  [plasma-activities-stats](#port-plasma-activities-stats), both required by it. Dolphin's and the
  file dialog's Recent Files and Recent Locations places are `recentlyused:/` URLs. Upstream's
  default, named.
- `-D BUILD_FUZZERS=OFF` — No thumbnailer fuzzing targets. Upstream's default, named.
- `-D USE_DBUS=ON` — Builds the parts that talk over D-Bus: the MTP, SMB and file-name search
  workers and the activities workers all need it. Upstream's default on Linux, named.
- `-D WITH_LIBPROXY=OFF` — No WPAD proxy detection in the Proxy page. It needs libproxy, which is
  not a port; the page still sets a manual proxy or a PAC URL.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Samba=ON` — Requires libsmbclient from [samba](#port-samba) for
  the `smb:/` worker, which upstream builds only when Samba is found. The worker requires
  KDSoapWSDiscoveryClient from [kdsoap-ws-discovery-client](#port-kdsoap-ws-discovery-client) to
  find Windows 10 and later machines over WS-Discovery, and [kdnssd](#port-kdnssd) for DNS-SD.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_libssh=ON` — Requires [libssh](#port-libssh) for the `sftp:/`
  worker, which upstream builds only when libssh is found. Once it is, the worker requires
  QCoro's Core component from [qcoro](#port-qcoro).
- `-D CMAKE_DISABLE_FIND_PACKAGE_TIRPC=ON` — No `nfs:/` worker. It browses NFSv2 and NFSv3 exports
  over ONC RPC through [libtirpc](#port-libtirpc), which this port does not name; NFS shares are
  mounted with the kernel client from [nfs-utils](#port-nfs-utils).
- `-D CMAKE_DISABLE_FIND_PACKAGE_libappimage=ON` — No AppImage thumbnails; libappimage is not a
  port.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Libmtp=ON` — Requires [libmtp](#port-libmtp) for the `mtp:/`
  worker.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_IMobileDevice=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_PList=ON` —
  Require [libimobiledevice](#port-libimobiledevice) and [libplist](#port-libplist), the two
  libraries the `afc:/` worker for iPhones and iPads needs.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Gperf=ON` — Requires [gperf](#port-gperf), which generates the
  `man:/` worker's lookup tables; without it upstream leaves that worker out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Taglib=ON` — Requires [taglib](#port-taglib) for the cover-art
  thumbnails of audio files.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON` — Requires [openexr](#port-openexr) for EXR
  thumbnails.
- `-D WITHOUT_X11=OFF` — Searches for X11, upstream's default, named; with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON` libX11 and [libXcursor](#port-libXcursor) are required
  for the thumbnails of X cursor themes, which are the files cursor themes still ship.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON` — Requires [libkexiv2](#port-libkexiv2), from which
  the thumbnailer reads the preview a JPEG or camera raw file carries instead of decoding the whole
  picture.

## Notes

The `man:/` worker renders pages with its own man2html converter and runs no formatter. The DjVu
thumbnailer runs `ddjvu` from [djvulibre](#port-djvulibre) at run time, which the configure does
not check.

The SMB worker's WS-Discovery search sends to UDP port 3702 and listens for the replies, so it
finds Windows machines only where the firewall lets those replies in.
