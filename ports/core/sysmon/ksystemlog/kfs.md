## About

KSystemLog reads the machine's log files and shows each kind in a tab of its own: the system log,
the kernel log, authentication, cron, daemons, ACPI, CUPS, Apache, Postfix and Samba. Lines are
parsed into date, host, process and message columns, can be filtered and searched, and new lines
appear as syslogd writes them. On KDOS the tabs read the files sysklogd writes: the system tab
`/var/log/messages`, the kernel tab what `dmesg` prints, authentication `/var/log/auth.log` and
cron `/var/log/cron`. The paths, and the tab the window opens on, come from
`/usr/share/kdos/xdg/ksystemlogrc`: upstream's defaults are Debian's `/var/log/syslog` for the
system and cron tabs and the journal as the first tab, and KSystemLog hides a tab whose files are
missing, so without that file the system and cron tabs would be gone and the window would open on
an empty log.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Journald=ON` — No journald mode. It reads the systemd journal
  through libsystemd, and KDOS has neither (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)); refused, a stray
  `libsystemd` in the build root cannot turn it on.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Audit=ON` — No audit mode, which reads the kernel audit log
  through libaudit. libaudit is not a port and KDOS runs no auditd.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`no-substitute-uid.patch` takes `X-KDE-SubstituteUID=true` out of the desktop entry, so the menu
starts KSystemLog as the user; it is to be offered upstream as a merge request against
`system/ksystemlog`. With the key, KIO starts it through `kdesu` as root, and `kdesu_stub` clears
`XDG_RUNTIME_DIR` and passes the Wayland socket's name as `DISPLAY`, so on the Wayland session the
root process reaches no display and aborts. There is no build option for the key.

## Notes

`/var/log/messages`, `/var/log/kern.log` and `/var/log/auth.log` are made readable by root alone
at boot, and `dmesg` is restricted to root, so an ordinary account reads only the cron tab: the
system and authentication tabs report that the file cannot be read, and the kernel tab is empty.
The window therefore opens on the cron tab. The X.org and X session tabs look for an X server's
logs, which this machine never writes.
