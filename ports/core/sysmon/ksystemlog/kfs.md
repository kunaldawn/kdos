## About

KSystemLog reads the machine's log files and shows each kind in a tab of its own: the system log,
the kernel log, authentication, cron, daemons, ACPI, CUPS, Apache, Postfix and Samba. Lines are
parsed into date, host, process and message columns, can be filtered and searched, and new lines
appear as syslogd writes them. On KDOS the system tab reads `/var/log/messages`, which sysklogd
writes.

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

## Notes

`/var/log/auth.log` is made readable by root alone at boot, so the authentication tab is empty for
an ordinary account. The desktop entry carries `X-KDE-SubstituteUID=true`, which makes KIO start
KSystemLog as root through `kdesu` when it is opened from the menu; `kdesu` is the Plasma session's
(kde-cli-tools). Started from a shell, it runs as the calling user. The X.org and X session tabs
look for an X server's logs, which this machine never writes.
