## About

Intel's share of [linux-firmware](#port-linux-firmware): the GuC, HuC and DMC firmware `i915` and
`xe` load, the `iwlwifi` Wi-Fi firmware with its top-level links, the Bluetooth `ibt-*` files, and
the IPU camera, VPU, ISH sensor-hub, QAT, `ice` and audio DSP firmware under `intel/`. 792 files
and links, 165,910,365 bytes. The installation medium carries it: without it Intel graphics from
Gen12 on run nothing on the GPU and Intel Wi-Fi does not come up. The SOF audio firmware is a
separate upstream, [sof-firmware](#port-sof-firmware).

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
