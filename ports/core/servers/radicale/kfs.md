## About

Radicale is a small CalDAV and CardDAV server written in Python: shared calendars, task lists and
address books for the machines on a LAN, with no account anywhere outside it. It keeps everything
as files under `/var/lib/radicale/collections`. The package adds a service script,
`77_radicale`, which runs it under the supervisor as the `radicale` account; other machines reach
it on port 5232 once the firewall's `caldav` name is on (see
[Services](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#services)).

## Flags

- `--prefix=$_home` — Install Radicale and the three Python packages it needs from its bundle into
  a prefix of its own, `/usr/lib/radicale`, instead of the shared `site-packages`. vobject is also
  vendored into `site-packages` by [khard](#port-khard), and two packages owning one path is a
  conflict that stops the install; under the prefix nothing else can own defusedxml, libpass or
  vobject (see
  [Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)).
- `--no-deps`, `--no-index`, `--find-links=vendor` — Install exactly the named distributions from
  the unpacked bundle and nothing from the network. requests, packaging, dateutil, pytz and six are
  ports, found in the shared `site-packages` through `depends`.
- `--no-build-isolation` — Build with the hatchling and setuptools already in the build root.

## Notes

The bundle, `radicale-vendor-<version>.tar.xz`, is what the recipe's `pypackages` key names:
defusedxml, libpass and a pinned vobject, downloaded without resolving dependencies, plus their
build requirements (see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).
vobject is pinned below its latest release, which reads its own version by importing the package;
that imports dateutil, and the isolated environment in which a source-only download builds
metadata has no dateutil, so the fetch fails. The pinned release declares its version statically.

pika, which Radicale declares, is imported only by its RabbitMQ hook, a plugin loaded by name from
the configuration. There is no RabbitMQ port for it to talk to, and pika is not installed.

`/usr/bin/radicale` is a wrapper script: it asks Python for the `site-packages` path under
`/usr/lib/radicale`, puts it on `PYTHONPATH`, and runs the real entry point from
`/usr/lib/radicale/bin`.

The shipped `/etc/radicale/config` serves the LAN with accounts. Upstream's default listens on
localhost and lets no one in; this one listens on every address on port 5232, takes accounts from
`/etc/radicale/users`, one `name:hash` line each (a hash that `openssl passwd -6` writes), and
gives each account only its own collections (`owner_only`). Upstream's full `config` and `rights`
examples go to `/usr/share/doc/radicale/`.

The service skips itself until the `radicale` account exists and `/etc/radicale/users` has a line:
with no account there is nobody to let in, and the port would otherwise be held open for nothing.
`setpriv` runs Radicale as its account; it stays in the foreground and logs to standard error,
which the supervisor keeps.

`postinstall.sh` creates the account and group and gives it `/var/lib/radicale` with mode 750. The
mode is set there because `kpkgadd` creates every directory 0755, whatever mode it was packaged
with. Like the other server ports it writes under `PKG_ROOT` and reads the ids from that root's
own `passwd`.
