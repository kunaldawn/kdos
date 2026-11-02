## About

KPty opens and drives pseudo-terminals for KDE programs: `KPty` and `KPtyDevice` own the master and
slave ends, and `KPtyProcess` starts a program on one, with its window size and terminal modes.
[konsole](#port-konsole) and [qmlkonsole](#port-qmlkonsole) run their shells through it,
[kdesu](#port-kdesu) talks to `sudo` through it, and [ark](#port-ark) drives interactive archivers
with it.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_UTEMPTER=ON` — Never look for libutempter, the setgid helper that
  lets an unprivileged program record a terminal session in `utmp`. It is not a port, so the switch
  keeps the result from depending on what is installed in the build root. Without it KPty writes
  the record itself through the C library's `utmpx` functions, which on musl record nothing, so a
  terminal tab does not appear in `who`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
