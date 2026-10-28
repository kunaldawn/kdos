## About

Net-SNMP is the [SNMP]{.idx} toolkit: the `libnetsnmp` library, the command-line tools (`snmpget`,
`snmpwalk`, `snmpset`, `snmptrap` and the rest), the agent `snmpd`, the trap receiver `snmptrapd`,
and the MIB files. On KDOS it is how a network UPS card, a managed switch or a printer is read;
nut's SNMP driver, hplip and powerman link the library.

## Flags

- `LDFLAGS=` — Run configure with an empty `LDFLAGS`, and pass the tree's flags to both `make` calls
  instead. The configure script writes its `LDFLAGS` into `net-snmp-config --ldflags` and `--libs`
  and into the `Libs.private` lines of `netsnmp.pc` and `netsnmp-agent.pc`, so a program built
  against the installed library would inherit the tree's linker flags; given to `make`, they apply
  to this build alone.
- `--with-defaults` — Take the default for every value configure would otherwise prompt for at the
  terminal, which an unattended build cannot answer. The prompted values the recipe does set are
  given beside it: `--with-default-snmp-version=3`, `--with-sys-contact=root@localhost`,
  `--with-sys-location=unknown`, `--with-logfile=/var/log/snmpd.log` and
  `--with-persistent-directory=/var/lib/net-snmp`.
- `--with-openssl` — Do the USM authentication and encryption of SNMPv3 with OpenSSL's libcrypto.
- `--enable-blumenthal-aes` — Offer AES-192 and AES-256 privacy as well as AES-128, following the
  Blumenthal draft.
- `--enable-ipv6` — Build the IPv6 transports.
- `--with-nl` — Build the netlink code that depends on libnl. Of the default MIB modules, IP-MIB's
  `ipDefaultRouterTable` is read from the kernel's routing table this way; without libnl, `snmpd`
  answers that table empty.
- `--without-pcre` — Build without PCRE. `snmpd.conf`'s `proc` directive then takes no regular
  expression, and the interface and filesystem filters fall back to the C library's POSIX
  `regex.h`. The configure script looks for PCRE1's `pcre.h`, which is not a port;
  [pcre2](#port-pcre2) installs `pcre2.h`, which Net-SNMP does not use.
- `--without-elf` — Link no libelf. On Linux configure's ELF check does nothing anyway: Linux is
  among the hosts that do not use `nlist()`, the one thing it would link libelf for.
- `--without-rpm` — With `--without-mysql`, `--without-libwrap` and `--without-systemd`, build no
  RPM package table in the host MIB, no MySQL trap storage, no TCP wrappers and no systemd
  notification. None of those is a port, and KDOS has no systemd; see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd).
- `--without-perl-modules` — With `--disable-embedded-perl`, `--without-python-modules` and
  `--disable-scripts`, build no Perl or Python binding, no Perl interpreter inside `snmpd`, and none
  of the Perl scripts (`mib2c`, `snmpconf`, `tkmib` and others).

## Notes

`build.sh` checks `net-snmp-config.h` after configure for `HAVE_LIBCRYPTO` and `HAVE_LIBNL3`, so a
build that silently lost OpenSSL or libnl stops there. The install runs with `make -j1`: its rules
race each other under a parallel make.

The recipe writes a service script, `/etc/init.d/71_snmpd.sh`. It skips the agent until
`/etc/snmp/snmpd.conf` exists and is not empty, because the file is not shipped and an agent with no
access lines answers nobody. Started, `snmpd` runs in the foreground (`-f`) under the supervisor and
logs to syslog's daemon facility (`-Lsd`); see
[rcS and the service scripts](../../../../docs/kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts).
`snmptrapd` has no script.
