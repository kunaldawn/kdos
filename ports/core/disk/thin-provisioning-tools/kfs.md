## About

thin-provisioning-tools checks, dumps, restores and repairs the on-disk metadata of the kernel's
device-mapper thin-provisioning, cache and era targets: `thin_check`, `thin_dump`, `thin_repair`,
`cache_check` and the rest. [lvm2](#port-lvm2) runs `thin_check` before it activates a thin pool and
`cache_check` before it activates a cached volume, and refuses the volume when the program cannot be
run, so these tools are what makes LVM thin pools and caches usable on KDOS. lvm2 is configured with
each tool's path under `/usr/sbin`, and the initramfs carries `thin_check` and `cache_check` for a
root on such a volume. It is a Rust program built from a vendored crate bundle.

## Flags

- `--features io_uring` — Compiles in the asynchronous I/O engine, built on the `io-uring` crate
  from the vendor bundle. It is used only when asked for with the hidden `--io-engine async` option;
  every tool still reads and writes through the synchronous engine by default, so a kernel without
  io_uring loses nothing.
- `CARGO_NET_OFFLINE=true` — Cargo's environment form of `net.offline`: it never reaches the
  network, the same as `--offline` on the command line.

## Notes

- **One binary, many names.** Upstream builds a single program, `pdata_tools`, which picks the tool
  to run from the name it is invoked by. The recipe installs it in `/usr/sbin` and makes each tool a
  symbolic link to it, the same links upstream's `make install` makes; the links are what make
  `thin_check` a command.
- **The manual pages are made one by one.** For each tool, `make man8/<tool>.8` renders the page
  from `man8/<tool>.txt` with the `bin/txt2man` script in the source, which calls `gawk` by name,
  so [gawk](#port-gawk) is in `depends`. The recipe does not run upstream's `make install`, whose
  rule builds the program with a plain `cargo build --release`, without the recipe's features and
  offline flags.
- **Built after lvm2.** `thin_migrate` talks to device-mapper through the `devicemapper` crate,
  which links lvm2's `libdevmapper`, found through pkg-config, so [lvm2](#port-lvm2) is in `depends`
  and lvm2 cannot ask these tools for their version when it is configured.
