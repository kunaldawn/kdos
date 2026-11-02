## About

libksane is the Qt 6 scanner widget over [ksanecore](#port-ksanecore): the device selector, the
option panels, the preview and the scan button. [skanlite](#port-skanlite) is that widget in a
window, and [kolourpaint](#port-kolourpaint) acquires pictures through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Wallet=ON` — Requires [kwallet](#port-kwallet), in which the
  widget keeps the passwords of scanners that ask for one. Upstream looks for it as optional, and
  without it the password is asked for each time the device is opened.
