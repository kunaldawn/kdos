## About

libidn2 converts internationalised domain names, host names written in Unicode such as
`bücher.example`, to and from the ASCII form DNS carries (`xn--bcher-kva.example`), following
IDNA2008, Punycode and Unicode's TR46 mapping. It installs the library and the `idn2` command. The
Unicode tables it needs come from [libunistring](#port-libunistring).

[curl](#port-curl) links it to resolve such names, [libpsl](#port-libpsl) uses it at run time to convert the
entries of a Public Suffix List file it loads, and [gnutls](#port-gnutls), [rsync](#port-rsync),
[iputils](#port-iputils), [dnsmasq](#port-dnsmasq), [whois](#port-whois), [lynx](#port-lynx),
[msmtp](#port-msmtp) and [gmime](#port-gmime) depend on it as well.

## Flags

- `--disable-nls` — build no message catalogues. No catalogue would ship anyway: the package's
  language prune removes every non-English one (see [Size and language
  policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  and with translation off the library has no use for GNU `libintl`; see [Gettext on
  musl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl).
- `--disable-valgrind-tests` — do not run the self-tests under Valgrind. By default configure looks
  for `valgrind` on `PATH` and, when it finds one, wraps the test suite in it. The
  [valgrind](#port-valgrind) port exists from `40_lang` on, so the switch keeps a rebuild's
  configure result the same whatever is installed. `build.sh` runs no test suite.

## Notes

The release tarball carries the API reference already generated as HTML, under `doc/reference/html`,
and `make install` copies it under `/usr/share/gtk-doc/html` whether or not gtk-doc is enabled. The
package prune removes HTML manuals only under `usr/share/doc` and TeX Live's documentation tree, so `build.sh` deletes
`usr/share/gtk-doc` itself. The manual pages and the Info manual stay.
