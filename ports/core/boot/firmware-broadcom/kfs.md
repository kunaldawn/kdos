## About

Broadcom's share of [linux-firmware](#port-linux-firmware): `brcm/` and `cypress/`, the `brcmfmac`
Wi-Fi and Bluetooth firmware, and `bnx2/`, `bnx2x/` and `tigon/` for NetXtreme Ethernet. 161 files
and links, 14,900,487 bytes. It is not on the installation medium; `kdos firmware sync` installs
it.

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
