## About

Kontrast checks whether a text colour and a background colour are far enough apart to read: it
shows the WCAG contrast ratio of a pair, suggests random accessible pairs, and keeps favourites.
It is a Kirigami application. The favourites live in an SQLite database written through
[futuresql](#port-futuresql), an asynchronous layer over Qt SQL's SQLite driver, with
[qcoro](#port-qcoro) for its coroutines; both are required by the configure.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The handbook is built through [kdoctools](#port-kdoctools), which upstream requires on every
platform but Android. The QML imports `org.kde.config`, which [kconfig](#port-kconfig) provides
and the configure does not check, so it is named in `depends`.
