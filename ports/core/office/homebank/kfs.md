## About

[HomeBank]{.idx} is a GTK 3 personal-finance program: accounts, scheduled transactions, budgets
and reports over a single `.xhb` file. It imports bank statements as QIF, CSV and, through
[libofx](#port-libofx), OFX and QFX.

## Flags

- `--disable-nls` — Build and install no translations. The packager removes every non-English
  catalogue whatever configure is told (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch saves build time and changes nothing else.
- `--with-ofx` — Import OFX and QFX statements through [libofx](#port-libofx). It is upstream's default,
  and does not make libofx required: configure turns the import off with only a message when
  `libofx/libofx.h` is missing, which the recipe's dependency on the port rules out.

## Patches

- `homebank-theme-files.patch` — adds two files the release tarball leaves out although its makefiles install them: `themes/Default/index.theme`, the description of HomeBank's private icon theme, and the symbolic icon `prf-transaction-transfer-symbolic.svg`, which `themes/hicolor-symbolic/Makefile.am` lists. It also makes the `install-icons` rule of `themes/Default/Makefile.in` create the theme's directory before it copies `index.theme` into it.

## Notes

`build.sh` tests after the install that `index.theme` and the transfer icon are in the package, so
the build fails if either of the patched files was not installed.

configure requires libsoup 3. HomeBank uses it only for the currency-rate update, which fails
offline and leaves the rates as entered.

`build.sh` writes the desktop entry with `StartupWMClass=fr.free.mdoyen.HomeBank`. The window's
Wayland app id is its GApplication id, which is neither the entry's file name nor its last
component; without the key the taskbar cannot match the window to its Start menu row.
