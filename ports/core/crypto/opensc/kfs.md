## About
OpenSC is the PKCS #11 module through which programs use a smart card's keys: `opensc-pkcs11.so`
speaks PIV, CAC, OpenPGP cards and many national eID cards to the reader that
[pcsc-lite](#port-pcsc-lite) and [ccid](#port-ccid) reach. Without it a card is visible to pcscd
and useless to everything else. Its `opensc.module` registers it with [p11-kit](#port-p11-kit),
so GnuTLS programs see the card's certificates and openconnect accepts a `pkcs11:` URI, and
`ssh -I /usr/lib/opensc-pkcs11.so` uses its keys; see
[Thunderbolt, phones, smart cards and firmware updates](../../../../docs/kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates).
The port also installs the card tools, `opensc-tool`, `pkcs11-tool`, `pkcs15-tool`,
`pkcs15-init`, `piv-tool` and the rest, with their manual pages and bash completions.

## Flags
- `--enable-openssl`, `--enable-zlib`, `--enable-readline`, `--enable-pcsc` — At `detect`,
  configure's default for the first three, a missing library is left out and configure goes on;
  named `yes`, a missing library stops configure. PC/SC is on by default. They give [openssl](#port-openssl) for the
  cryptography of most card drivers, [zlib](#port-zlib) for compressed certificates on cards,
  [readline](#port-readline) for `opensc-explorer`'s line editing, and PC/SC for the readers.
- `--enable-sm` — Secure messaging, the encrypted channel some cards, national eIDs among them,
  require between the host and the card; upstream's default, written out.
- `--enable-man`, `--disable-doc` — Generate the manual pages with `xsltproc` from
  [libxslt](#port-libxslt), and install no HTML documentation.
- `--with-xsl-stylesheetsdir=/usr/share/xml/docbook/xsl-stylesheets-nons-1.79.2` — The DocBook
  stylesheets the manual pages are made with, from [docbook-xsl](#port-docbook-xsl). Left to
  detect them, configure looks only in a set list of other systems' directories (Debian's,
  MacPorts', Fink's and `/usr/share/sgml/docbook`), and with `--enable-man` it stops when it finds
  none.
- `--with-pcsc-provider=libpcsclite.so.1` — The PC/SC library OpenSC opens at run time, by its
  versioned soname, which is upstream's default for Linux written out.
- `--disable-notify` — No `opensc-notify`, the desktop notifier for card insertion and removal,
  built on GIO notifications.
- `--disable-autostart-items` — Install no XDG autostart entry for `pkcs11-register`, which
  would add OpenSC's module to the user's Firefox, Chromium and Thunderbird NSS databases at every
  login; upstream's default, written out.
- `--disable-openpace` — No PACE, the password-authenticated channel of the German eID card and
  others, which needs the OpenPACE library; it is not in the tree.
- `--disable-tests`, `--disable-cmocka` — Build neither upstream's test programs under
  `src/tests` nor the cmocka suites in `src/tests/p11test` and `src/tests/unittests`.

## Notes
`make install` puts `opensc-notify`'s desktop entry, bash completion and manual page in place even
with `--disable-notify`, so `build.sh` deletes those three files. It deletes them with `rm` and no
`-f`, so the build fails if a release stops installing any of them.

The stylesheet directory is named with its version. A [docbook-xsl](#port-docbook-xsl) update that
renames it makes configure stop with `invalid` at the `xsl-stylesheets` check until this path
follows it.
