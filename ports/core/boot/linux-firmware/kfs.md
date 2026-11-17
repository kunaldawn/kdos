## About

linux-firmware is the collection of [firmware]{.idx} files that device drivers load into the
hardware they drive: Wi-Fi and Bluetooth chips, network cards, sound DSPs and, on most current
machines, the GPU. Without it a real machine has no Wi-Fi, no Bluetooth and, on most current
graphics, no accelerated display: `amdgpu` brings up no GPU without its blobs, and the system
falls through to `simpledrm` on the EFI framebuffer, which has no render node, so the compositor
draws with the pixman software renderer and no application gets hardware EGL; `xe`, and `i915`
on the Intel graphics that submit work through the GuC, run nothing on the GPU without the GuC
firmware.

This port is a meta: it installs nothing itself and depends on the eight vendor ports that split
upstream's tree between them, so `kpkg install linux-firmware` or
`sudo kdos update install linux-firmware` is the whole tree. Each vendor port builds the same
tarball and keeps its share; [firmware-map](#port-firmware-map) builds it once more and records
which port ships each file, for [`kdos firmware sync`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-firmware).
The installation medium carries the four vendors most machines need, and every other one is a
prebuilt package from the release's binary host (see
[The base image](../../../../docs/kdos/03-architecture/packaging.md#the-base-image)).

| Port | Takes | Files and links | Bytes | On the medium |
|---|---|---|---|---|
| [firmware-intel](#port-firmware-intel) | `i915/`, `xe/`, `intel/` (iwlwifi, Bluetooth `ibt-*`, IPU, VPU, ISH, QAT, ice, the AVS and catpt DSPs), the top-level `iwlwifi-*` and `qat_*` links, `isci/` | 792 | 165,910,365 | yes |
| [firmware-amd](#port-firmware-amd) | `amdgpu/`, `radeon/`, `r128/`, `amd/`, `amdnpu/`, `amdtee/`, `amd-ucode/` | 977 | 46,613,917 | yes |
| [firmware-mediatek](#port-firmware-mediatek) | `mediatek/` and Ralink's top-level `rt2*`, `rt3*` and `rt73` files | 135 | 40,047,901 | yes |
| [firmware-realtek](#port-firmware-realtek) | `rtw88/`, `rtw89/`, `rtlwifi/`, `rtl_bt/`, `rtl_nic/`, `realtek/` | 165 | 9,580,394 | yes |
| [firmware-nvidia](#port-firmware-nvidia) | `nvidia/` | 491 | 113,769,285 | no |
| [firmware-qualcomm](#port-firmware-qualcomm) | `ath*/`, `ar3k/`, `qca/`, `qcom/` and Atheros' top-level `ath3k`, `ar5523`, `ar7010`, `ar9170`, `ar9271`, `carl9170`, `htc_*` and `wil6210` files | 428 | 83,369,680 | no |
| [firmware-broadcom](#port-firmware-broadcom) | `brcm/`, `cypress/`, `bnx2/`, `bnx2x/`, `tigon/` | 161 | 14,900,487 | no |
| [firmware-misc](#port-firmware-misc) | everything else: Cirrus and TI sound amplifiers, Marvell, Mellanox, Netronome, QLogic and Chelsio server networking, TV tuners, older Wi-Fi | 3,427 | 121,511,339 | no |

The bytes are the files each package installs, measured from a build of upstream's 20260916
release; each package also carries the licence texts, about 0.97 MB. The medium's four come to
262,152,577 bytes and the four left off it to 333,550,791. AMD's CPU microcode is in
firmware-amd, under `amd-ucode/`, and `090_initramfs.sh` concatenates it into the early microcode
archive (see [Microcode](../../../../docs/kdos/03-architecture/boot-and-init.md#microcode)); the
medium therefore always carries firmware-amd.

The files are code for another processor, so they are one of the payloads that are installed as
upstream publishes them rather than compiled here; see
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

## Notes

Every vendor port and firmware-map carries `linux-firmware-$version.tar.xz` under this port's
file name and checksum, its `version` equals this port's, and all of them carry
`group = linux-firmware`, so the version checker offers the set only as one bump (see
[A second build of the same source](../../../../docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)).
They share one `build.sh`, byte for byte, and `testing/preflight.sh` fails a port whose copy
differs from firmware-map's, a vendor the rule names that is not a port, and a vendor port this
meta's `depends` leaves out. That one script:

1. Installs every file and link upstream's `WHENCE` lists into a staging tree with upstream's own
   `copy-firmware.sh --zstd`, so each `File:` entry lands as `<name>.zst` and each `RawFile:`
   entry, the AMD microcode among them, uncompressed. The kernel decompresses them as it loads
   them: the [linux](#port-linux) port's `kdos.config` sets `CONFIG_FW_LOADER_COMPRESS` and
   `CONFIG_FW_LOADER_COMPRESS_ZSTD`, and a kernel built without them finds none of these files.
   [zstd](#port-zstd) is in each port's `depends` for this.
2. Deletes the firmware no x86 kernel loads, in order. Under `qcom/`, every entry except
   `aic100`, `aic200`, `qdu100`, the `sdx*` directories and the `prog_firehose_*` files, which are
   the Qualcomm firmware kept as loadable by an x86 kernel, and every top-level symlink into
   `qcom/` this leaves without a target. Of the Mellanox Spectrum switch firmware, every
   `mlxsw_spectrum*.mfa2` except one version for each of the first three Spectrum generations:
   the file the kernel's `mlxsw` driver names in its `MLXSW_SP1_FW_FILENAME`,
   `MLXSW_SP2_FW_FILENAME` and `MLXSW_SP3_FW_FILENAME`. The driver requests that exact file and
   no other; every Spectrum-4 image goes, because the driver names no firmware file for that
   generation. Of the Marvell Prestera firmware, the two oldest images, `v2.0` and `v3.0`, which
   the kernel's driver never asks for: it asks for its supported version, `v4.1`, and falls back
   only to the one before it, `v4.0`.
3. Fails if any symlink in the staging tree is left without a target.
4. Gives every file and link to one vendor with `fw_vendor`, a `case` over its path relative to
   `/lib/firmware`, first match winning, anything unmatched going to firmware-misc. A link is
   given to the vendor of the file it resolves to, never by its own path, so the top-level
   `iwlwifi-*` links travel with `intel/iwlwifi/` and the TI amplifier links at the top with
   `ti/audio/`, and no package holds a link to another package's file; the vendor port's build
   fails if one does.
5. In a vendor port, copies the files and links that are its own into `$PKG/lib/firmware`, and
   fails when there are none, which is what a misspelt vendor in the rule produces. In
   firmware-map, writes one `<port> <path>` line per file and link instead, the path without
   `.zst`, as a driver requests it, and sorted by path, to `/usr/share/kdos/firmware-map`.
6. Installs `WHENCE`, the top-level licence files and the `LICENSES/` directory into
   `/usr/share/licenses/<port>`, and fails if `WHENCE` names a licence file that is not there.

The `mlxsw` versions kept are the kernel's choice, written into `build.sh` as a list of files to
keep. When a new [linux](#port-linux) names different versions, that list has to follow it, or
the firmware the new driver requests is among the files deleted. The Prestera images are deleted
by name, so a kernel that asks for a newer one is unaffected.

A rule moved from one vendor to another changes which package owns the files, so every port of
the set is rebuilt together, as a version bump does. Each vendor port names `linux-firmware` in
`replaces`: on a root whose `linux-firmware` package still owns the files, the vendor port takes
them instead of being refused, and the meta's own upgrade then installs no files. The meta cannot
be in the vendor ports' `depends`, which `replaces` otherwise asks for, because it depends on
them; it owns no path, so installing it again never contends with them.

The packages install into `/lib/firmware`. `/lib` is a link to `/usr/lib` in the KDOS root, so
the files land in `/usr/lib/firmware`, the directory the image phase reads microcode from.
