## About

Eclipse Mosquitto is an MQTT broker, the publish-and-subscribe message server that sensors,
microcontrollers and home-automation devices report to, with the `mosquitto_pub` and
`mosquitto_sub` clients, `libmosquitto`, and `mosquitto_ctrl` for administering a running broker.
A broker on the machine makes a sensor network testable with no cloud account. The package adds a
service script, `73_mosquitto`, which runs the broker under the supervisor once
`/etc/mosquitto/mosquitto.conf` exists; see
[Services](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#services).

## Flags

- `-DCMAKE_INSTALL_SYSCONFDIR=/etc` — Install the example configuration under `/etc/mosquitto`
  rather than `/usr/etc`.
- `-DWITH_WEBSOCKETS=ON`, `-DWITH_WEBSOCKETS_BUILTIN=ON` — MQTT over WebSockets, with Mosquitto's
  own WebSocket code rather than libwebsockets; both are upstream's defaults. It listens only where
  a listener is configured with `protocol websockets`.
- `-DWITH_DOCS=ON` — Build the manual pages from DocBook with `xsltproc`, which is why
  [libxslt](#port-libxslt) and [docbook-xsl](#port-docbook-xsl) are in `depends`. Upstream's
  default.
- `-DWITH_TESTS=OFF` — Build no tests, which upstream builds by default against GoogleTest.
- `-DWITH_TLS=ON` — TLS listeners and clients, on [openssl](#port-openssl); upstream's default.
- `-DWITH_HTTP_API=ON` — The broker's HTTP API listener, on [libmicrohttpd](#port-libmicrohttpd).
  The library is found by a silent `find_library`, and the listener is dropped without a word when
  it misses, which is why libmicrohttpd is in `depends`.
- `-DHTTP_API_DIR=` — Empty the directory the HTTP API serves files from by default. Its cache
  default is the literal string "Default http_api directory", and an `http_api` listener with no
  `http_dir` of its own would `realpath()` that, fail, and refuse to start.
- `-DWITH_PLUGIN_PERSIST_SQLITE=ON` — Build the plugin that keeps the broker's persistent state (messages,
  retained messages, client sessions) in
  [sqlite](#port-sqlite). It is a required `find_package`, so a missing sqlite stops configure.
- `-DWITH_CTRL_SHELL=ON` — `mosquitto_ctrl`'s interactive shell, upstream's default. It needs a line-editing library,
  found silently and dropped when missing; with both installed it takes
  [libedit](#port-libedit)'s `editline/readline.h` ahead of readline.
- `-DWITH_PLUGIN_EXAMPLES=OFF` — Build none of the example plugins, which are on by default.
- `-DWITH_SYSTEMD=OFF` — No systemd notification; KDOS has no systemd (see
  [No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DWITH_SRV=OFF` — The clients do no DNS SRV lookup of the broker; upstream's default.
- `-DWITH_DLT=OFF` — No logging to the GENIVI Diagnostic Log and Trace daemon; upstream's default.

## Notes

The install writes only `mosquitto.conf.example`. With no configuration at all the broker listens
on loopback alone, which `mosquitto` run by hand already covers, so the service skips itself until
`/etc/mosquitto/mosquitto.conf` exists. That file is what says devices on the LAN are meant to
reach the broker: a `listener 1883` line in it, and the firewall's `mqtt` name switched on.

`postinstall.sh` creates the `mosquitto` group and system account (home `/var/lib/mosquitto`, no
login shell) unless they exist. Started as root, the broker drops to that account; without it
Mosquitto falls back to `nobody`, which every other daemon asking for an unprivileged account
shares. Everything is written under `PKG_ROOT`, so an install into another root, such as an A/B update,
gets the account too, and the guard reads that root's `/etc/group` and `/etc/passwd` itself:
`getent` would answer for the running system instead. The package carries the home directory and
`postinstall.sh` gives it to the account, so a configuration that turns persistence on can name
`persistence_location /var/lib/mosquitto/`.

cJSON is a required dependency with no option of its own (`find_package(cJSON REQUIRED)`);
[cjson](#port-cjson) is in `depends`.
