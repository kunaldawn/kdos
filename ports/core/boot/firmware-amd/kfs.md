## About

AMD's share of [linux-firmware](#port-linux-firmware): the `amdgpu` and `radeon` firmware, the
NPU's (`amdnpu/`), the security processor's and TEE's (`amd/`, `amdtee/`), and AMD's CPU microcode
under `amd-ucode/`, which `090_initramfs.sh` puts into the early microcode archive. 977 files and
links, 46,613,917 bytes. The installation medium carries it, for the GPU and for the microcode.

## Flags

- `--zstd` — Compresses every `File:` entry of `WHENCE` as `copy-firmware.sh` copies it; see
  [linux-firmware](#port-linux-firmware), whose note describes the build every port of the set
  shares.
