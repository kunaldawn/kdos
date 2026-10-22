# Linux API headers

The Linux API headers describe the kernel's interface to user space: the system call numbers, the
structures `ioctl` and `stat` fill in, the constants of every subsystem. musl is compiled against
them in the next step, and every program that talks to a device or a socket includes them. This
step installs them from the kernel source of the [`linux`]{.idx} port into `build/fs/usr/include`. This page
shows the commands of `script/phases/10_bootstrap/010_linux_headers.sh` and explains them.

**Source:** the archive of the [linux](#port-linux) port\
**Installs into:** `build/fs/usr/include`\
**Approximate build time:** 0.2 SBU

## Installation of the Linux API headers

<!-- kfs:include script/phases/10_bootstrap/010_linux_headers.sh lines=23-27 pin=a801b2404770 -->

The whole kernel source is unpacked to install its headers. Nothing is configured and nothing is
compiled for the target; the only program built is [`unifdef`]{.idx}, the helper the header install runs,
with the container's compiler.

### The meaning of the make options

`ARCH=x86_64`
: Install the headers of 64-bit x86, the `asm` directory among them, whatever the machine running
  the build is.

`headers_install`
: Install the user-space API headers, the `uapi` half of the kernel's headers, with the parts that
  are only for the kernel's own use stripped out by the `unifdef` helper. The target then copies
  only the `.h` files to the destination with `rsync`, which the container provides. It adds and
  overwrites; it never deletes a header already there.

`INSTALL_HDR_PATH=$SYSROOT/usr`
: Write the headers to `build/fs/usr/include`: `linux`, `asm`, `asm-generic` and the directory of
  each subsystem that has one, such as `drm`, `sound` and `rdma`.

## Contents

**Installed headers:** `/usr/include/{asm,asm-generic,drm,linux,misc,mtd,rdma,regulator,scsi,sound,video,xen}/*.h`
and the other subsystem directories the kernel exports.

## Built again later

The [linux](#port-linux) port, in `60_kernel`, builds the kernel itself, and after it runs the same
target into its package: `make INSTALL_HDR_PATH=$PKG/usr headers_install`. The headers are the same
files, written by the same source. Until that phase installs the package, the headers in
`build/fs/usr/include` belong to no package; once it does, kpkg adopts them as the `linux`
package's files (see [Part III](intro.md#what-the-bootstrap-leaves-behind)). The port builds
natively inside the chroot, so it gives no `ARCH`.

## See also

- [Part III](intro.md): the bootstrap
- [musl libc](020-musl-libc.md): the library compiled against these headers
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md#the-kernel-60_kernel): the kernel phase that builds the `linux` port

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 9.* Previous: [8. The file system](000-file-system.md) · [Contents](../README.md) · Next: [10. musl libc](020-musl-libc.md)
