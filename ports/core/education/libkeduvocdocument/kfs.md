## About

libkeduvocdocument reads and writes KDE's vocabulary documents: KVTML, and the word-list formats
it imports, such as CSV, Pauker, Vokabeln and XDXF. [parley](#port-parley) and
[kwordquiz](#port-kwordquiz) keep their lessons in it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
