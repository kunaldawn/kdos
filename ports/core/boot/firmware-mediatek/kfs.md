## About

MediaTek's share of [linux-firmware](#port-linux-firmware): everything under `mediatek/`, the
`mt76` Wi-Fi and Bluetooth firmware that is the Wi-Fi of most AMD laptops, and the Ralink files at
the top of the tree (`rt2561`, `rt2860`, `rt73` and the rest), Ralink being MediaTek's. 135 files
and links, 40,047,901 bytes. The installation medium carries it.

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
