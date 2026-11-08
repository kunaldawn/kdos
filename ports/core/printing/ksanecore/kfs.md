## About

ksanecore drives SANE scanners for Qt 6 programs, with no user interface: it lists devices, reads
and sets their options and runs scans, through [sane-backends](#port-sane-backends).
[libksane](#port-libksane) builds its scanner widget on it, and Skanpage uses it directly.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
