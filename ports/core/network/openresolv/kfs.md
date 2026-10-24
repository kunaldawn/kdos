## About

openresolv's `resolvconf` is the one writer of `/etc/resolv.conf`. NetworkManager, the fallback
`dhcpcd` and `wg-quick` each hand it the servers of a link, and it merges them into the file and
into the configuration of the subscribers named in `SUBSCRIBERS`: the C library's resolver
(`libc`), `dnsmasq`, `named`, `pdnsd`, `pdns_recursor` and `unbound`. `libc` always runs; the
others act only when their key is set in `/etc/resolvconf.conf`. Each restarts its service after a
change when that service is running, so it reads the new servers: `libc`'s is `nscd`.

## Flags

- `RESTARTCMD` — on both `make` lines: the command a subscriber runs to restart its service, with
  the service's name in `$1`. Unset, `resolvconf` probes the running system for an init system it
  knows and finds `/sbin/service`, which it calls as Red Hat's, `service <name>` and then
  `service <name> restart`; `ksvc` takes the command first, `service status <name>`, and answers
  the other order with its usage, so no subscriber would ever be restarted. The value restarts a service only when
  `service status` reports it running, as the probed commands do.

## Notes

`RESTARTCMD` is passed to `make` rather than to `configure --restartcmd`. `configure` writes the
value into `config.mk` with `echo`, and whether `echo` turns `\\` into `\` depends on which shell
`/bin/sh` is, so the same flag would give a different `resolvconf` under `dash` and `bash`. On the
`make` line the value passes through two layers, the same under any shell:

- make expands `$$1` to `$1`.
- The Makefile substitutes the value into `resolvconf.in` with `sed 's:@RESTARTCMD@:…:g'`, so the
  value holds no `:`, `\&` stands for `&` and `\\` for `\`.

The result in `/usr/sbin/resolvconf` is
`RESTARTCMD="/usr/sbin/service status \$1 >/dev/null 2>&1 && /usr/sbin/service restart \$1"`.
That line is an unquoted shell assignment in `resolvconf.in`, so the value carries its own double
quotes, and `\$1` keeps the name from being expanded until each subscriber `eval`s the command.
`resolvconf -R` prints the command as the subscribers run it.
