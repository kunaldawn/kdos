## About
Nettle is a low-level cryptographic library: hash functions, ciphers, MACs, and, in its companion
library `libhogweed` over [gmp](#port-gmp), public-key algorithms. It has no protocol of its own;
[gnutls](#port-gnutls) is built on it, as are [dnsmasq](#port-dnsmasq)'s DNSSEC validation,
[chrony](#port-chrony), [openconnect](#port-openconnect) and the FileZilla libraries. The port also
installs the small command-line tools `nettle-hash`, `nettle-pbkdf2`, `sexp-conv`, `pkcs1-conv`
and `nettle-lfib-stream`.

## Flags
- `--disable-openssl` — Build the benchmark programs without their OpenSSL comparison glue. It
  touches only `nettle-benchmark` and `hogweed-benchmark`, neither of which is installed.
- `--disable-documentation` — Build and install no Texinfo manual. At configure's default, `auto`,
  it is built when `makeinfo` is found, so the package's contents would depend on what the build
  root holds.
- `CFLAGS="$CFLAGS"` — Given to both `make` runs, so the tree's compiler flags are the whole
  `CFLAGS`. With GCC, nettle's configure appends `-ggdb3` and its warning flags to `CFLAGS` last,
  and a value on the `make` command line overrides the one configure wrote into the Makefile.
