## About

GDBM is GNU's small key-value database library: a single file of hashed records, opened by key,
with no server. It is in `30_foundation` because the two interpreters there use it. Perl's
`GDBM_File` module links `libgdbm`, and its `NDBM_File` and `ODBM_File` modules link
`libgdbm_compat`; python3's `dbm.gnu` links `libgdbm` and its `dbm.ndbm` links `libgdbm_compat`.
avahi stores its service-type database in a GDBM file (`--enable-gdbm` in its recipe). The package
also installs `gdbmtool`, an interactive shell for a GDBM file, and `gdbm_dump` and `gdbm_load`.

## Flags

- `--enable-libgdbm-compat` — Also build `libgdbm_compat`, which implements the old Unix `dbm` and
  `ndbm` interfaces on top of GDBM and installs `dbm.h` and `ndbm.h`. musl has no `ndbm`, so this
  library is what perl's `NDBM_File` and `ODBM_File` and python3's `dbm.ndbm` are built against.
- `--with-readline` — Link `gdbmtool` with [readline](#port-readline) for line editing and
  history, and stop the configure if readline is missing rather than building without it.
- `--disable-nls` — Build no message catalogues. With NLS on, gettext's `libintl.h` renames every
  call to `libintl_gettext`, which only the [libintl](#port-libintl) port defines, and neither
  package is in `depends`; whether gdbm linked libintl would then follow the order the two happen
  to be built in, in a phase where python3 pulls gdbm in early. See
  [Gettext on musl](../../../../docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl).
