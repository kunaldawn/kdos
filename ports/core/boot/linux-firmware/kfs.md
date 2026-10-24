## About

linux-firmware is the collection of [firmware]{.idx} files that device drivers load into the
hardware they drive: Wi-Fi and Bluetooth chips, network cards, sound DSPs and, on most current
machines, the GPU. Without it a real machine has no Wi-Fi, no Bluetooth and no accelerated
display: `amdgpu` from Vega (GFX9) on, Xe, and Gen12 and later `i915` will not bring the GPU up
without their blobs, and the system falls through to `simpledrm` on the EFI framebuffer, a picture
with no EGL and no compositor. AMD's CPU microcode is in this tree too, under `amd-ucode/`, and
`090_initramfs.sh` copies it into the early microcode archive (see
[Microcode](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#microcode)).

The files are code for another processor, so they are one of the payloads that are installed as
upstream publishes them rather than compiled here; see
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
The port installs upstream's whole tree with upstream's own `copy-firmware.sh`, then deletes the
firmware no x86 kernel loads.

## Flags

- `--zstd` — Makes `copy-firmware.sh` compress every file with zstd as it copies it, so each blob
  is installed as `<name>.zst`. The kernel decompresses them as it loads them: the [linux](#port-linux)
  port's `kdos.config` sets `CONFIG_FW_LOADER_COMPRESS` and `CONFIG_FW_LOADER_COMPRESS_ZSTD`. A
  kernel built without them finds none of these files. [zstd](#port-zstd) is in `depends` for
  this.

## Notes

The deletions after the copy, in order:

- Under `qcom/`, every entry except `aic100`, `aic200`, `qdu100`, the `sdx*` directories and the
  `prog_firehose_*` files, which are the Qualcomm firmware the recipe keeps as loadable by an x86
  kernel. Every top-level symlink into `qcom/` that this leaves without a target is removed with
  it.
- Of the Mellanox Spectrum switch firmware, every `mlxsw_spectrum*.mfa2` except one version for
  each of the first three Spectrum generations: the file the kernel's `mlxsw` driver names in its
  `MLXSW_SP1_FW_FILENAME`, `MLXSW_SP2_FW_FILENAME` and `MLXSW_SP3_FW_FILENAME`. The driver requests
  that exact file and no other.
- Of the Marvell Prestera firmware, the two oldest images, which the kernel's driver never asks
  for: it asks for its supported version and falls back only to the one before it.

The `mlxsw` versions kept are the kernel's choice, written into `build.sh` as a list of files to
keep. When a new [linux](#port-linux) names different versions, that list has to follow it, or
the firmware the new driver requests is among the files deleted. The Prestera images are deleted
by name, so a kernel that asks for a newer one is unaffected.

After the deletions the build fails if any symlink in the tree is left without a target, rather than
shipping a link a driver would follow to nothing. It also installs `WHENCE`, the top-level licence
files and the `LICENSES/` directory into `/usr/share/licenses/linux-firmware`, and fails if
`WHENCE` names a licence file that is not there.

The package installs into `/lib/firmware`. `/lib` is a link to `/usr/lib` in the KDOS root, so
the files land in `/usr/lib/firmware`, the directory the image phase reads microcode from.
