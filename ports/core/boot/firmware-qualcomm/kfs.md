## About

Qualcomm's and Atheros' share of [linux-firmware](#port-linux-firmware): the `ath10k`, `ath11k` and
`ath12k` Wi-Fi firmware, `ath6k`, `ath9k_htc`, the Atheros USB files at the top of the tree, `qca/`
Bluetooth, and what is kept of `qcom/` (the AI 100 accelerators and the modem firehose
programmers). 428 files and links, 83,369,680 bytes. It is not on the installation medium; `kdos
firmware sync` installs it.

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
