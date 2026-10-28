## About

MiniDLNA (ReadyMedia) is a DLNA/UPnP media server: it indexes the music, pictures and video in
the directories its configuration names, and serves them to televisions, consoles and players on
the local network. The port ships its own start script, `/etc/init.d/84_minidlna.sh`, which
starts `minidlnad` under supervision as the unprivileged `minidlna` account and skips the service
until `/etc/minidlna.conf` exists and is not empty; the upstream example is installed as
`/usr/share/doc/minidlna/minidlna.conf.example`. Other machines reach it only once the `dlna`
firewall name is opened; see [Opening a service](../../../../docs/kdos/02-user-guide/administration.md#opening-a-service)
and [firewall](../../../../docs/kdos/04-programs/daemons.md#firewall).

## Flags

- `export CFLAGS="$CFLAGS -fcommon"` — Place uninitialised global variables in a common block, so
  one defined without `extern` in more than one object file is merged at link time instead of
  failing the link with a multiple-definition error, which GCC's default `-fno-common` gives.
- `ac_cv_lib_avahi_client_avahi_threaded_poll_new=no` — Answer configure's Avahi probe with *no*,
  so the server is built without announcing itself over mDNS/DNS-SD even when
  [avahi](#port-avahi) is in the build root. Avahi is not in `depends`. DLNA clients find the
  server through its own SSDP announcements on UDP 1900.
- `--with-db-path=/var/lib/minidlna` — The default directory for the media database and the
  album-art cache. Upstream's default is `/var/cache/minidlna`; `/var/lib/minidlna` is the
  directory the package creates and `postinstall.sh` gives to the `minidlna` account.
- `--with-log-path=/var/log/minidlna` — The default log directory, also created by the package and
  owned by the account; upstream's default is `/var/log`, which the account cannot write. The start
  script's `-S` keeps `minidlnad` in the foreground and sends its log to standard output for the
  supervisor, so the directory is written only when the daemon is started by hand without `-S`.
- `--with-os-name=KDOS`, `--with-os-url=https://github.com/kunaldawn/kdos` — The model description
  (`MiniDLNA on KDOS`) and model URL the server publishes in its UPnP device description.
  Upstream takes the name from `uname -s` and uses Netgear's web site as the URL.
- `--disable-tivo`, `--disable-netgear`, `--disable-readynas` — TiVo support, and the Netgear and
  ReadyNAS device identities, which replace the device description's manufacturer and model with
  Netgear's. All three are off in upstream's default; naming them keeps them off.

## Patches

- `ffmpeg7.patch` — Taken from Arch Linux (its `Patch-Source:` line). Reads an audio stream's channel count from `codecpar->ch_layout.nb_channels` instead of `codecpar->channels`, a field the FFmpeg API of this tree does not have; without it `libav.h` does not compile.
- `libav-fix.patch` — Drops `@LIBAVUTIL_LIBS@` from the link line of the `testupnpdescgen` test program in `Makefile.am`. `configure.ac` never substitutes that variable, so after `autoreconf -fi` the reference would stay in the generated Makefile as literal text.

## Notes

`autoreconf -fi` runs after both patches because `libav-fix.patch` edits `Makefile.am`; without
it the shipped `Makefile.in` would still carry the reference. That is why
[autoconf](#port-autoconf), [automake](#port-automake) and [libtool](#port-libtool) are in
`depends`.

`postinstall.sh` makes the `minidlna` group and account inside `$PKG_ROOT`, then gives
`/var/lib/minidlna` and `/var/log/minidlna` to it by the numeric ids it reads from that root's
`/etc/passwd`, so an install into a root that is not the running system gives the directories
to the right account. The start script runs `minidlnad -S -u minidlna`, so the daemon writes its
database after dropping to that account; see
[postinstall.sh](../../../../docs/kdos/05-developer/writing-ports.md#postinstallsh) and
[System accounts](../../../../docs/kdos/03-architecture/security-model.md#system-accounts).
