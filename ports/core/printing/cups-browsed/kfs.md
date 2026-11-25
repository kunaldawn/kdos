## About
cups-browsed watches the network for printers announced over DNS-SD by
[avahi](#port-avahi) and turns each one it finds into a local [CUPS](#port-cups) queue, driverless
through [libcupsfilters](#port-libcupsfilters) and [libppd](#port-libppd), so a network printer is
there to print to with nothing added by hand. `/etc/init.d/81_cups-browsed.sh` runs it under
supervision after `80_cups.sh`; it never forks, so it needs no foreground flag.

## Flags
- `-Wl,-z,stack-size=8388608`, added to `LDFLAGS` — ask for an 8 MiB stack in the program's
  `PT_GNU_STACK` header, the size a thread gets by default on glibc, where upstream develops. musl
  gives each new thread 128 KiB unless that header asks for more, and cups-browsed starts the
  threads that resolve a printer and create its queue (`resolve_callback`, `create_queue`) with
  default attributes. The queue-creation thread overruns 128 KiB inside libcupsfilters: as soon as
  an IPP printer is announced the daemon dies with SIGSEGV at a fault address equal to its stack
  pointer, the supervisor restarts it, and it dies again every few seconds, so no queue is ever
  added. `readelf -lW /usr/sbin/cups-browsed` shows the size as the `GNU_STACK` entry's `0x800000`.
- `--enable-avahi` — Browse for printers through Avahi's client library.
- `--with-rcdir=no` — Install no System V init script; the init script is KDOS's own.
- `--with-cups-rundir=/run/cups` and `--with-cups-domainsocket=/run/cups/cups.sock` — Reach the
  scheduler on the socket the [cups](#port-cups) port's `cupsd` listens on.

## Notes
cups-browsed adds queues to `cupsd` and does nothing without it, so it runs only while `cupsd`
does. `80_cups.sh` starts it once `cupsd` is supervised, unless
`/etc/service.disabled/cups-browsed` exists, and stops it before `cupsd`; `sudo service start
cups` brings both up. Started on its own while `cupsd` is not supervised, the init script prints
`[SKIP] cups-browsed: CUPS is off` and starts nothing.
