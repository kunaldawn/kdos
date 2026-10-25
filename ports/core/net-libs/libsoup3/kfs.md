## About
libsoup is GNOME's HTTP client and server library, built on GIO; this port is its `libsoup-3.0`
API. On KDOS it serves GStreamer's `souphttpsrc`
and adaptive streaming demuxers, which load it at run time, and links into
[geoclue](#port-geoclue), [gvfs](#port-gvfs), [webkitgtk](#port-webkitgtk) and other GNOME-stack
applications. Its `https://` connections go through the GIO TLS module
[glib-networking](#port-glib-networking) installs.

## Flags
- `-Dgssapi=enabled` — Negotiate (Kerberos/SPNEGO) HTTP authentication through
  [krb5](#port-krb5). Each `enabled` feature below stops `meson setup` when what it needs is
  missing, where the default `auto` would build without it.
- `-Dntlm=enabled` — NTLM HTTP authentication. `meson setup` stops if it cannot find the
  `ntlm_auth` program below.
- `-Dntlm_auth=/usr/bin/ntlm_auth` — Samba's `ntlm_auth` helper, which libsoup runs to answer an
  NTLM challenge with the user's own credentials. Meson looks the program up when the build is
  configured and compiles its full path into the library; upstream's default is the bare name,
  found through `PATH`. [samba](#port-samba) is in `depends` so that the helper is there.
- `-Dbrotli=enabled` — Decode `Content-Encoding: br` responses with [brotli](#port-brotli).
- `-Dzstd=enabled` — Decode `Content-Encoding: zstd` responses with [zstd](#port-zstd).
- `-Dtls_check=false` — Skip the configure test that compiles and runs a program asking GIO
  whether its default TLS backend supports TLS. Upstream offers the switch to packagers,
  because glib-networking is needed only at run time; it is in `depends` here, so the installed
  library has its TLS module.
- `-Dintrospection=enabled` — Generate `Soup-3.0.typelib` and its GIR, for language bindings.
- `-Dvapi=disabled` — Generate no Vala bindings.
- `-Dautobahn=disabled` — Build none of the Autobahn WebSocket conformance tests, and with
  `-Dpkcs11_tests=disabled` none of the PKCS #11 tests; `-Dtests=false` already builds no tests.
- `-Dsysprof=disabled` — Do not link the sysprof profiler's capture library.
