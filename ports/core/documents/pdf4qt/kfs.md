## About
[PDF4QT]{.idx} is a PDF suite on Qt 6 with its own rendering library: a viewer, an editor that
annotates, redacts, fills forms and signs, PageMaster for merging, splitting and reordering pages,
and Diff for comparing two documents. Each is its own program (`Pdf4QtViewer`, `Pdf4QtEditor`,
`Pdf4QtPageMaster`, `Pdf4QtDiff`), with `Pdf4QtLaunchPad` to choose between them. It requires
[blend2d](#port-blend2d), a 2D vector rasteriser, and [onetbb](#port-onetbb) at configure time,
and reads text aloud through Qt Speech.

## Flags
- `-D PDF4QT_BUILD_TESTS=OFF` — no unit tests, which upstream builds by default.
- `-D PDF4QT_INSTALL_DEPENDENCIES=OFF` — do not copy the third-party libraries into the install,
  and with `-D PDF4QT_INSTALL_QT_DEPENDENCIES=OFF` not Qt either. Upstream turns both on for a
  vcpkg build, which bundles its libraries; here every library is a port.
- `-D PDF4QT_INSTALL_TO_USR=OFF` — do not add a `usr/` level of upstream's own under the install
  prefix, which with `CMAKE_INSTALL_PREFIX=/usr` would install into `/usr/usr`. Upstream turns it
  on only for a vcpkg build.

## Patches
- `openssl4.patch` — ports the certificate and signature code in `Pdf4QtLibCore` to OpenSSL 4. Pointers to `X509_NAME`, `X509_NAME_ENTRY`, `ASN1_STRING`, `ASN1_TYPE` and `X509_PUBKEY` that OpenSSL 4's accessors return as `const` are declared `const`; an `ASN1_STRING`'s bytes and length are read through `ASN1_STRING_get0_data()` and `ASN1_STRING_length()` instead of its `data` and `length` fields; and a new self-signed certificate builds its name in a fresh `X509_NAME` and sets it with `X509_set_subject_name()`, because under OpenSSL 4 the subject that `X509_get_subject_name()` returns is `const` and cannot be written to. Without it the build fails against the tree's [openssl](#port-openssl), which is OpenSSL 4.

## Notes
Upstream installs a desktop entry for each program. `build.sh` writes its own over them, adding
`%f` to the Viewer's and Editor's `Exec` and `%F` to Diff's, so a file opened from the file
manager reaches the program, and a `StartupWMClass` for each. Upstream's entries all claim
`application/pdf`; the port's keep that only for the three that take a file, so PageMaster and the
LaunchPad are not offered for opening a PDF, and the LaunchPad's entry has `NoDisplay=true`, which
the menu then leaves out.
