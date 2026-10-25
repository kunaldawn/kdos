## About
bandwhich shows, live in the terminal, which processes and which connections are using the
network and how much bandwidth each takes. It sniffs packets on an interface and matches them to
processes through `/proc`. It is built from its
[Rust vendor bundle](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)
with no network.

## Flags
- `BANDWHICH_GEN_DIR` — bandwhich's `build.rs` renders a manual page and a completion file for
  every shell clap knows into this directory, falling back to cargo's hashed `OUT_DIR` under
  `target/` when it is unset. Pointing it at `gen/` in the source tree gives `build.sh` a fixed
  path to install `bandwhich.1` from. The completion files land there too and are not installed.

## Notes
Capturing packets needs `CAP_NET_RAW` and `CAP_NET_ADMIN`, and matching a connection to its
process needs `CAP_SYS_PTRACE` and `CAP_DAC_READ_SEARCH` to read other processes'
`/proc/<pid>/fd/`. The package grants none of them: `bandwhich` is not setuid and carries no file
capability, so it runs as root or with the four capabilities set on the file yourself, as
upstream's README shows with `setcap`. See
[setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries).
