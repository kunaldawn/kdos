## About

GNU gettext is the toolchain for translating programs: `xgettext` extracts the strings to
translate, `msgmerge` and the other `msg*` programs maintain the `.po` catalogues, `msgfmt`
compiles them, and `autopoint` and `gettextize` add gettext's autotools machinery to a source
tree. It also installs [libtextstyle]{.idx}, the library that colours terminal output, which
[bison](#port-bison) depends on gettext for, and `libasprintf` and `libgettextpo`.

KDOS ships no translations (see
[Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)), but a great many builds still run these tools: an
`autoreconf` that calls `autopoint`, a meson `i18n` module that runs `msgfmt` over a desktop
entry or an AppStream file, an `intltool` or `itstool` step. Many ports depend on it for that
alone.

The C library side is different from a glibc distribution. musl carries its own `gettext()` and
its own `libintl.h`, and this package, built `--disable-nls`, installs neither a `libintl.h` nor a
`libintl` library. The [libintl](#port-libintl) port builds the GNU runtime library alone from
this same tarball for the programs that need its symbols; see
[Gettext on musl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl).

## Flags

- `--disable-nls` — Builds the programs without their own translations, and installs no
  `libintl`; the libintl port provides that library.
- `--disable-java` — Builds none of gettext's Java sources, such as the `intl-java` runtime
  classes. [openjdk](#port-openjdk) is a port, but of a later phase.
- `--disable-csharp` — Builds none of the C# support, such as the `intl-csharp` runtime. Neither
  Mono nor .NET is a port.
- `--without-git` — Keeps configure from choosing the git format for `autopoint`'s infrastructure
  archive. Under `--with-xz` the choice is xz whatever this switch says.
- `--with-xz` — Stores the infrastructure archive that `autopoint` unpacks as `archive.dir.tar.xz`.
  Left to probe, configure picks the format from whichever of xz, git and bzip2 the build root
  holds, and `autopoint` then needs that same program on every machine it runs on; xz is in
  `depends`, so the format is the same on every build (see
  [An optional tool found in the tree](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#an-optional-tool-found-in-the-tree)).
- `--without-emacs` — Installs no Emacs PO mode. Emacs is not a port.
- `--with-included-libxml` — Builds the copy of libxml2's parser that gettext carries into its
  own libraries, for the tools that read XML (ITS rules, Glade and AppData files). The system
  [libxml2](#port-libxml2) cannot be used: libxml2 depends on doxygen, doxygen on bison, and bison
  on gettext, so linking it here would be a dependency cycle.
- `--with-libunistring-prefix=/usr` — Links the system [libunistring](#port-libunistring), which is
  in `depends`, instead of the parts of it gettext can compile in.
- `--with-libncurses-prefix=/usr` — Searches for ncurses in `/usr/include` and `/usr/lib`.
  libtextstyle reads the terminal's capabilities through it.
- `--without-selinux` — Builds without SELinux support. libselinux is not a port: SELinux is
  outside what KDOS builds (see
  [Features whose library has no port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port)).
- `--without-libsmack` — Builds without Smack label support, for the same reason.

## Notes

`rm -f "$PKG/usr/share/gettext/javaversion.class"` removes a compiled Java class that
`--disable-java` still installs. It is not built here: the release tarball carries it already
compiled, beside its `javaversion.java`, and libgettextlib hands it to a Java virtual machine to
learn the machine's version. A prebuilt object that fits none of the exempt classes is deleted
from the package (see
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).

[acl](#port-acl) and [attr](#port-attr) are in `depends`. configure turns on support for access
control lists and extended attributes when it finds their libraries, so with both always present
libgettextlib links `libacl` and `libattr`, and its file-copying code carries both. Without them in
`depends`, the result would follow whatever the build root held.
