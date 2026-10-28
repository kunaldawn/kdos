## About
FUSE lets an ordinary program serve a filesystem: the kernel passes each file operation on a FUSE
mount to the process behind it, through `/dev/fuse`. This port is libfuse 3, the library such a
program is written against, and `fusermount3`, the setuid helper that performs the mount and
unmount for a user who may not call `mount(2)` themselves. sshfs, gocryptfs, fuse-overlayfs, gvfs,
the document portal, `rclone mount` and `restic mount` all go through it; see
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).

## Flags
- `--libexecdir=/usr/lib` — Where internal helper programs would go. libfuse 3 installs none,
  so the switch changes nothing in the package.
- `-Dinitscriptdir=` — Empty: install no SysV init script. Upstream's default writes
  `/etc/init.d/fuse3`, which loads the `fuse` module and mounts the `fusectl` control filesystem.
  KDOS's `rcS` runs only scripts named `NN_name.sh` in that directory, so the file would sit there
  and never run.
- `-Dudevrulesdir=/lib/udev/rules.d` — Install `99-fuse3.rules`, which gives `/dev/fuse` mode 0666,
  into this directory. Left empty, meson asks pkg-config for udev's directory instead.
- `-Denable-io-uring=true` — Build the FUSE-over-io-uring transport, in which requests and replies
  pass through io_uring queues instead of `read` and `write` on `/dev/fuse`. On is upstream's
  default; see Notes for what it actually requires.
- `-Dexamples=false` — Build none of the example filesystems.
- `-Dtests=false` — Compile no test programs.

## Notes
The io_uring transport is built only when meson finds both [liburing](#port-liburing) and libnuma
(from [numactl](#port-numactl)) and a test program using `IORING_SETUP_SQE128` links. When any of
those fails, configure turns the transport off without an error, whatever `-Denable-io-uring`
says. `numactl` is in `depends` for that libnuma, and the `readelf -d … | grep -q
'NEEDED.*\[liburing\.so'` line after the install stops the build when `libfuse3.so` does not link
liburing, instead of shipping a library without the transport.

The install script, with upstream's default `useroot`, makes `fusermount3` owned by root and setuid,
and creates a `/dev/fuse` device node under `$PKG`. `build.sh` removes `$PKG/dev`: the running
system's `/dev` is a devtmpfs the kernel fills, and the udev rule gives the node its mode. The
setuid bit stays.

`fusermount` is a symbolic link to `fusermount3`, because some FUSE clients (restic's among them)
run the unversioned name.
