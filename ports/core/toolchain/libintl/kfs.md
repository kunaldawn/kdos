## About
libintl is the run-time half of GNU gettext: the library a program calls to look up the
translation of a message (`gettext`, `ngettext`, `dgettext` and the rest) in the message
catalogues installed for the current locale. This port builds that library alone, from the same
gettext tarball and version as the [gettext](#port-gettext) port.

It is in KDOS for programs built against musl. musl's libc has `gettext()` and its relatives, but not the
`libintl_gettext`, `libintl_ngettext` and other `libintl_`-prefixed names that GNU's
`libintl.h` maps them to, which only GNU's libintl defines. The gettext port is built
`--disable-nls` and installs no libintl, so a program whose objects call those names, such as
[newsboat](#port-newsboat), or whose build links libintl whenever it finds one, as
[gdk-pixbuf](#port-gdk-pixbuf)'s and [fcitx5](#port-fcitx5)'s do, links against this one. The
diagnosis and the choice between a build switch and this library are in
[Gettext on musl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl).

## Flags
- `--enable-nls` — Turn national language support on, against the tree's `--disable-nls`
  policy. gettext's configure chooses how libintl is provided only when NLS is on, so
  `--with-included-libintl` takes effect only with this switch.
- `--with-included-libintl` — Build the GNU libintl in the tarball's `intl/` directory, whatever
  configure's probes would find in the C library or an installed libintl. This is the library the
  port installs.
- `--disable-rpath` — Record no run-time library search path in what is built. The library
  lives in `/usr/lib`, which the dynamic linker searches anyway.
- `--disable-java` — Build none of gettext-runtime's Java sources, the `libintl.jar` classes.
- `--disable-csharp` — Build no C# support, the `GNU.Gettext.dll` assembly.
- `--without-emacs` — gettext-runtime's configure has no such option: it belongs to
  gettext-tools, where it leaves out the Emacs PO mode, and gettext-runtime's configure, which
  has subdirectories to configure and so checks no option names, accepts it silently. It changes
  nothing in this build.

## Notes
configure runs at the level of `gettext-runtime`, which configures `intl/` as a subdirectory
with the same options; `make -C intl` then builds and installs only the library. Everything
else in gettext-runtime, `gettext`, `ngettext`, `envsubst` and libasprintf, belongs to the
gettext port, and installing it here as well would put two packages on the same paths.

`intl`'s install writes GNU's `libintl.h` too, and `build.sh` deletes it from `$PKG`.
`/usr/include/libintl.h` is [musl](#port-musl)'s, and kpkg refuses a package that would
overwrite a path another package owns ([Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)).
A program compiled against musl's header calls the plain names; the library exports those as
well as the `libintl_` ones.
