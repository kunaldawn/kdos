## About

[numbat]{.idx} is a calculator whose type system knows physical units: it adds a length to a length,
converts metres per second to kilometres per hour, and refuses to add a voltage to a resistance,
which `bc` does without complaint. It is an interactive shell and a one-shot command (`numbat -e
'...'`). The recipe writes a terminal desktop entry for it, named Calculator.

## Flags

- `--bin numbat` — Build only the `numbat` program from the workspace, whose other members are the
  language library and the exchange-rate crate it links.

## Notes

The crates come from the vendor bundle, `numbat-vendor-<version>.tar.xz`, pinned with its own
`sha256 =` line and unpacked at the top of the tree; cargo builds `--frozen --offline` from it (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

The unit, constant and function definitions are Numbat source files, the `modules` tree. The library
compiles a copy into the binary, and the program also looks for modules on disk:
`NUMBAT_MODULES_PATH`, then `~/.config/numbat/modules`, then `/usr/share/numbat/modules`, which
`build.sh` fills from the same release. A module found on disk is used ahead of the compiled-in
copy, so the installed tree and the binary must come from the same version.

The default features are built, so numbat fetches the European Central Bank's daily exchange rates
over the network: the interactive shell starts the download in the background when it opens, and a
one-shot command fetches when a calculation first uses a currency. `fetching-policy` under
`[exchange-rates]` in `~/.config/numbat/config.toml` changes that, and `never` turns it off. Upstream's release profile sets
`strip = true`, so cargo builds the binary without its symbol table.
