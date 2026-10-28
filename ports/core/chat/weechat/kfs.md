## About
WeeChat is a chat client for the terminal, built around IRC and extended through plugins and
scripts: the relay plugin lets a remote interface or another WeeChat attach to a running session,
and scripts in Python, Perl, Ruby, Lua, Tcl and Scheme add the rest. The port builds the curses
client `weechat`, the daemon-style `weechat-headless`, every plugin upstream ships except the PHP
and JavaScript ones, the manual pages, and a desktop entry that opens it in a terminal as *IRC*.
TLS and the ECDSA mechanism of IRC SASL come from [gnutls](#port-gnutls); the encryption of
secured data and the hashing behind the SCRAM mechanisms of IRC SASL come from
[libgcrypt](#port-libgcrypt).

## Flags
- `-DLIBDIR=/usr/lib` — WeeChat's own name for the library directory, given although it matches
  upstream's default under the `/usr` prefix; the plugins go in `$LIBDIR/weechat/plugins`.
- `-DENABLE_MAN=ON`, `-DENABLE_DOC=OFF` — Build the manual pages, which are generated with
  [asciidoctor](#port-asciidoctor), and not the HTML guides (the user's guide, the plugin API
  reference, the scripting guide and the rest). Upstream's default for both is off.
- `-DENABLE_NLS=OFF` — No message catalogues: the interface is in English, and the build needs no
  gettext. Upstream's default is on; the tree's
  [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
  would remove the catalogues in any case.
- `-DENABLE_HEADLESS=ON` — Also build `weechat-headless`, which runs a session with no interface
  for a relay client to attach to.
- `-DENABLE_ZSTD=ON` — Zstandard compression, for rotated log files and for relay messages.
- `-DENABLE_CJSON=ON` — The relay's `api` protocol, an HTTP REST API, whose JSON is read and
  written with [cjson](#port-cjson).
- `-DENABLE_SCRIPT=ON`, `-DENABLE_SCRIPTS=ON` — The script manager (`/script`), and the
  scripting plugins as a group; each language below is switched on its own.
- `-DENABLE_PYTHON=ON`, `-DENABLE_PERL=ON`, `-DENABLE_LUA=ON`, `-DENABLE_RUBY=ON`,
  `-DENABLE_GUILE=ON` — The Python, Perl, Lua, Ruby and Scheme plugins, against
  [python3](#port-python3), [perl](#port-perl), [lua](#port-lua), [ruby](#port-ruby) and
  [guile](#port-guile).
- `-DENABLE_TCL=ON`, `-DTCL_LIBRARY=/usr/lib/libtcl9.0.so`, `-DTCL_INCLUDE_PATH=/usr/include` —
  The Tcl plugin, with the library and the header directory of [tcl](#port-tcl) named instead of
  left to CMake's `FindTCL` to search for.
- `-DENABLE_PHP=OFF` — No PHP plugin. It is on by default upstream, and PHP is not in the tree.
- `-DENABLE_JAVASCRIPT=OFF` — No JavaScript plugin, which needs the V8 engine; upstream's default
  is off as well.
- `-DENABLE_SPELL=ON`, `-DENABLE_ENCHANT=ON` — The spell checker, through
  [enchant](#port-enchant). Upstream's default, with `ENABLE_ENCHANT` off, links aspell directly.

## Notes
Each scripting plugin that is switched on finds its language with a `REQUIRED` CMake lookup, and
the Tcl one stops on a missing library with an error. Taking one of those languages out of
`depends` therefore makes the build fail, rather than leaving the plugin out; to drop a language,
switch its plugin off as well.
