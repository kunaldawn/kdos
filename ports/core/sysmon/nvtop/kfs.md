## About

nvtop is a `top` for GPUs: a terminal program that shows each graphics card's load, memory and
clock, and the processes using it. For AMD and Intel GPUs it reads per-process engine time from
the DRM `fdinfo` files under `/proc/<pid>/fdinfo`, the same source
[kdos-energyd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/daemons.md#kdos-energyd)'s GPU column reads, so
the two agree. `build.sh` writes its desktop entry, which opens it in a terminal from the menu as
*GPU Monitor*.

## Flags

Upstream turns most backends on by default for Linux, so each is named here, and the binary
carries readers only for the drivers an x86-64 KDOS machine can have
([x86-64 only](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/known-gaps.md#x86-64-only)).

- `-DNVIDIA_SUPPORT=OFF` — The NVIDIA backend, which loads NVML from NVIDIA's proprietary driver.
  That driver is not something this tree can build, so the backend would have nothing to load.
- `-DAMDGPU_SUPPORT=ON` — AMD GPUs through the `amdgpu` kernel driver and
  [libdrm](#port-libdrm).
- `-DINTEL_SUPPORT=ON` — Intel GPUs through the `i915` and `xe` kernel drivers.
- `-DMSM_SUPPORT=OFF`, `-DPANFROST_SUPPORT=OFF`, `-DPANTHOR_SUPPORT=OFF`, `-DV3D_SUPPORT=OFF` —
  The Adreno (`msm`), Mali (`panfrost`, `panthor`) and Raspberry Pi (`v3d`) drivers, which exist
  only on Arm systems.
- `-DROCKCHIP_SUPPORT=OFF` — The Rockchip NPU reader, which upstream enables only on Arm Linux.
- `-DASCEND_SUPPORT=OFF`, `-DTPU_SUPPORT=OFF`, `-DMETAX_SUPPORT=OFF`, `-DENFLAME_SUPPORT=OFF` —
  Huawei Ascend NPUs (through `libdcmi.so` from the Ascend driver), Google TPUs (through
  `libtpuinfo.so`), MetaX GPUs (through `libmxsml`) and Enflame GCUs (through `libefml`). Each
  vendor library comes from a vendor driver stack that is not a port.
- `-DUSE_LIBUDEV_OVER_LIBSYSTEMD=ON` — Discover devices through libudev, which
  [eudev](#port-eudev) provides, even if a libsystemd is found. KDOS has no systemd
  ([principles](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)); with the switch
  the build cannot pick up a stray libsystemd, and with neither library it fails, since the AMD
  and Intel backends need one of them.
