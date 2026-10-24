## About
containers-common is KDOS's own port with no upstream source: it writes `/etc/containers`, the
configuration that [podman](#port-podman), [buildah](#port-buildah) and [skopeo](#port-skopeo) all
read, so one package owns it and all three depend on it. Four files make it: the engine's
`containers.conf`, the image store's `storage.conf`, `registries.conf` for short image names, and
the image trust policy `policy.json`. What each sets is tabled in
[Containers](../../../../docs/kdos/06-reference/configuration.md#containers).

## Notes
Each value follows from something else in the system:

- `cgroup_manager = "cgroupfs"`, because there is no systemd to delegate a cgroup;
  `events_logger = "file"` for the same reason, there being no journal.
- `runtime = "crun"`, `network_backend = "netavark"` with `firewall_driver = "nftables"`, and
  `default_rootless_network_cmd = "pasta"` name the ports the tree carries:
  [crun](#port-crun), [netavark](#port-netavark) with [aardvark-dns](#port-aardvark-dns), and
  [passt](#port-passt).
- `storage.conf` pins the overlay driver mounted through
  [fuse-overlayfs](#port-fuse-overlayfs), the one `depends` entry, because a live session's home
  sits on the boot overlay and the kernel will not stack an overlay's writable layer on another;
  see [Storage drivers](../../../../docs/kdos/04-programs/kdos-appbox.md#storage-drivers) for when
  an installed system switches a user to the kernel's own driver.
- `policy.json` is upstream's default, `insecureAcceptAnything`, which checks no signature, and
  without any `policy.json` every pull refuses to start; see
  [Containers](../../../../docs/kdos/03-architecture/security-model.md#containers) in the security
  model.
