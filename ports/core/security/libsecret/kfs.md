## About

libsecret is the client library of the freedesktop Secret Service: a program stores and looks up
passwords with it, and libsecret sends those requests over the session bus to whichever program
owns `org.freedesktop.secrets`. The package also carries `secret-tool`, the same operations from a
shell. It holds no Secret Service itself; in KDOS the provider is [keepassxc](#port-keepassxc)
with its Secret Service integration turned on, and while it is not running every lookup fails and
the program asks again (see
[A saved login needs KeePassXC running](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/known-gaps.md#a-saved-login-needs-keepassxc-running)).
[chromium](#port-chromium), [webkitgtk](#port-webkitgtk), [qtkeychain](#port-qtkeychain),
[kwallet](#port-kwallet), [gvfs](#port-gvfs), [remmina](#port-remmina) and many other GTK programs
link it.

## Flags

- `export CFLAGS="$CFLAGS -include fcntl.h"` — Include `<fcntl.h>` at the top of every C file.
  `secret-file-collection.c` uses `O_CREAT` and `O_RDWR` and includes only `<sys/file.h>` for
  them; glibc's `<sys/file.h>` pulls in `<fcntl.h>`, musl's does not, and the build stops at the
  undeclared names.
- `-Dcrypto=libgcrypt` — The library behind the transport encryption between libsecret and the
  Secret Service, here [libgcrypt](#port-libgcrypt). Upstream's default; the other choices are
  GnuTLS or none.
- `-Dintrospection=true`, `-Dvapi=true` — Generate the GObject introspection data and the Vala
  bindings, so Python, GJS and Vala programs can use the library. Both are upstream's defaults.
- `-Dmanpage=true` — Build `secret-tool`'s manual page from DocBook with `xsltproc`, upstream's default; this is why
  [libxslt](#port-libxslt), [docbook-xsl](#port-docbook-xsl) and [docbook-xml](#port-docbook-xml)
  are in `depends`.
- `-Dgtk_doc=false` — No API reference; it is built with gi-docgen, which is not a port.
- `-Dbash_completion=enabled` — Install `secret-tool`'s bash completion, and fail configure if
  [bash-completion](#port-bash-completion) is not found, instead of upstream's `auto`.
- `-Dtpm2=false` — Build without the TPM2 Software Stack, which upstream leaves off.
- `-Dpam=false` — Build no PAM module; upstream's default. libsecret's module is a client of gnome-keyring-daemon's
  control socket, and there is no gnome-keyring port.
- `-Dtest_setup=disabled` — Do not look for `dbus-run-session` to set up a test environment; the
  port runs no tests.
- `-Ddebugging=false` — Build without upstream's debug code; the default.
