## About

virtiofsd is the daemon behind virtio-fs, the way a QEMU guest mounts a directory of the host. QEMU
exposes a `vhost-user-fs` device, and this daemon, started beside the guest and connected to it
through a Unix socket, serves the shared directory into it; QEMU has no server of its own for it.
One daemon serves one directory. It confines itself with seccomp and drops capabilities with
libcap-ng, which is why [libseccomp](#port-libseccomp) and [libcap-ng](#port-libcap-ng) are in
`depends`.

## Notes

The crates come from the vendor bundle, `virtiofsd-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor` and unpacked into the top of the source tree; the build runs
`--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

`build.sh` writes `/usr/share/qemu/vhost-user/50-virtiofsd.json`, the vhost-user descriptor of
type `fs` that names the daemon. When a guest's virtio-fs `<filesystem>` share names no
`<binary path=…>` of its own, libvirt looks the daemon up through such descriptors, and the guest
fails to start with `Unable to find a satisfying virtiofsd` when none qualifies. Upstream's own `50-virtiofsd.json` names `/usr/libexec/virtiofsd`; the one written here is
the same file with the path where this package installs it, `/usr/bin/virtiofsd`.
