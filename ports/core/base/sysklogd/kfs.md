## About

sysklogd provides `syslogd`, the [system log]{.idx} daemon. It receives the messages programs send
to `/dev/log` and reads the kernel's own messages itself, so there is no separate `klogd`, and it
writes them to files under `/var/log` according to `/etc/syslog.conf`. It is KDOS's replacement
for journald (see
[No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).

`/etc/init.d/22_syslog.sh` starts it at boot under the supervisor, before the network comes up,
with `-F` so that it stays in the foreground where the supervisor can watch it, and with
`-P /run/syslogd/syslogd.pid`, because its default pid file, `/run/syslogd.pid`, is the path the
supervisor keeps its own process id in, and with `-8`, which writes the bytes 0x80 to 0x9F as they
arrive. Without it `syslogd` replaces each with `M-` and the matching ASCII character, and those
are the bytes that follow the first byte of most UTF-8 punctuation: an em dash then leaves its
first byte alone in the file, which is no longer valid UTF-8, and `grep` reports
`binary file matches` for the whole of `/var/log/messages`. Which messages go
to which file is in [Logs](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#logs).

## Flags

- `--runstatedir=/run` — Where `syslogd` writes its cache, `syslogd.cache`, and its pid file,
  `syslogd.pid`, when `-P` names no other. The default is `$localstatedir/run`, `/var/run`, which on KDOS is a link to
  `/run`, so the switch names the directory the files actually land in.
- `--without-systemd` — Installs no systemd service file. KDOS has no systemd.
- `--without-logger` — Builds no `logger` of sysklogd's own. The [util-linux](#port-util-linux)
  port installs `/usr/bin/logger`, and the two would claim the same path.

## Notes

The recipe writes `/etc/syslog.conf` itself, and its comments are what a reader of the installed
file sees. Two of its rules are easy to get wrong when editing it:

- Rotation is set by the global `rotate_size` and `rotate_count` directives, not by options after a
  file name. A path written where a `facility.priority` selector belongs makes `syslogd` report
  `unknown priority name ""` for that line. Each file rotates at 1 MB and keeps six old copies (`.0`,
  then `.1.gz` to `.5.gz`: `rotate_count 5` counts the compressed ones), so
  no `logrotate` job exists; on the live medium `/var/log` is in RAM, and an unbounded log would
  use up memory.
- `secure_mode 2` opens no network socket at all. Without it, and with no `listen` line,
  `syslogd` opens UDP port 514 and writes any host's messages into these files; `secure_mode 1`
  stops the listening but keeps forwarding to a remote `@host` action.
