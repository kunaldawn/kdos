## About

NVIDIA's share of [linux-firmware](#port-linux-firmware): everything under `nvidia/`, the signed
firmware `nouveau` needs to accelerate a GPU from second-generation Maxwell (GM20x) on, and the
GSP firmware it needs to initialise a Turing or later GPU at all. 491 files and links,
113,769,285 bytes. It is not on the installation medium: a live session on such a GPU draws with
the software renderer, and `kdos firmware sync` installs this port. `kdos doctor` warns when
`nouveau` is bound without it.

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
