## About
[libvirt]{.idx} is a library and a daemon, `libvirtd`, that manage virtual machines, their
networks and their storage through one API, with `virsh` as its command line. In KDOS it drives
[QEMU](#port-qemu)/KVM guests, and it is what [virt-manager](#port-virt-manager) talks to, through
`qemu:///system` or `qemu:///session`. The package installs two services, `62_virtlogd` and
`63_libvirtd`, and upstream's polkit rule that lets members of the `libvirt` group manage system guests (see
[Services](../../../../docs/kdos/02-user-guide/administration.md#services)).

## Flags
- `-Dremote_default_mode=legacy` — clients connect to `libvirtd`'s socket. The default, `direct`,
  makes them connect to `virtqemud` and the other per-driver daemons, which nothing in KDOS
  starts, so every `qemu:///system` connection would fail with the socket missing.
- `-Dinit_script=none` — install no init scripts of upstream's (systemd or OpenRC); `build.sh`
  writes KDOS's own two.
- `-Dqemu_user=qemu` — with `-Dqemu_group=kvm`: system-mode guests run as their own account
  rather than as root, and libvirt's DAC driver gives each guest's disk images to that account.
  `postinstall.sh` creates `qemu` with `kvm` as its group, the group udev gives `/dev/kvm`.
- `-Ddriver_qemu=enabled` — with the other drivers named `enabled`: the QEMU driver, the remote
  driver and `libvirtd`, the NAT and bridge network driver (with [dnsmasq](#port-dnsmasq) for DHCP
  and DNS), host interfaces through udev, secrets, and the test driver virt-manager opens as
  `test:///default`.
- `-Ddriver_bhyve=disabled` — and every other hypervisor driver named `disabled` (Cloud
  Hypervisor, ESX, Hyper-V, libxl, LXC, OpenVZ, VirtualBox, VMware, Virtuozzo): left to `auto`,
  each would be built whenever its probe happened to succeed.
- `-Dstorage_dir=enabled` — with `-Dstorage_fs`, `-Dstorage_disk`, `-Dstorage_lvm`,
  `-Dstorage_scsi`, `-Dstorage_mpath` and `-Dstorage_iscsi_direct`: storage pools in directories,
  file systems, whole disks, LVM, SCSI hosts, multipath devices, and iSCSI targets reached through
  [libiscsi](#port-libiscsi).
- `-Dstorage_iscsi=disabled` — no iSCSI pool through `iscsiadm`, which needs open-iscsi, not a
  port; `iscsi-direct` is the iSCSI pool.
- `-Dstorage_gluster=disabled` — with `-Dstorage_rbd`, `-Dstorage_vstorage` and `-Dstorage_zfs`
  disabled, and `-Dglusterfs`, `-Dnetcf`, `-Dnumad`, `-Dopenwsman`, `-Dsanlock`, `-Daudit`,
  `-Dselinux`, `-Dapparmor` and `-Ddtrace`: backends and features whose library or tools are not ports.
- `-Dnumactl=disabled` — with `-Dlibssh`, `-Dcurl`, `-Dfuse`, `-Dwireshark_dissector` and
  `-Dbash_completion` disabled: no NUMA placement of guests, no libssh transport (`-Dlibssh2` is
  the SSH transport), no curl or FUSE support, no Wireshark dissector for libvirt's
  protocol, and no bash completion. Each library is a port the build does not link.
- `-Dfirewall_backend_priority=nftables` — with `-Dfirewalld=disabled`: the network driver writes
  its rules into an nftables table of its own beside `/etc/nftables.conf`. nwfilter still runs
  `ebtables`, `iptables` and `ip6tables`, the nftables-backed commands of the
  [iptables](#port-iptables) port.
- `-Dnbdkit=enabled` — with `-Dnbdkit_config_default=disabled`: [nbdkit](#port-nbdkit), through
  [libnbd](#port-libnbd), can serve a guest's network disk when the guest's XML or `qemu.conf`
  asks for it; QEMU's own block drivers stay the default.
- `-Ddocs=enabled` — build the manual pages (`virsh(1)` and the rest) with `rst2man` and
  `xsltproc`, and generate `/usr/share/libvirt/api/libvirt-api.xml`. The Python binding generates
  itself from that file and cannot be built without it.
- `-Dnss=disabled` — no `libnss_libvirt`, a glibc NSS module; musl has no NSS to load it into.
- `-Dssh_proxy=disabled` — no ssh-proxy for SSH over vsock, which writes a client configuration
  under `/etc/ssh`.
- `-Dsysctl_config=disabled` — no sysctl file for post-copy live migration.
- `-Dhost_validate=enabled` — build `virt-host-validate`, which checks the host for what each
  driver needs.
- `-Dlogin_shell=disabled` — with `-Dpm_utils=disabled`: no `virt-login-shell`, and no pm-utils
  for host suspend.
- `-Dno_git=true` — with `-Dgit_werror=disabled`: do not update git submodules, and do not add
  `-Werror` as a build from git would.
- `-Drunstatedir=/run` — the run-time state directory; each daemon creates its own under
  `/run/libvirt` when it starts.

## Notes
`build.sh` deletes `/usr/lib/sysusers.d`, systemd's account mechanism, which meson installs
whatever `-Dinit_script` says; `postinstall.sh` makes the `libvirt` group and the `qemu` account
instead. It also deletes the empty `/run` tree meson installs: `/run` is a tmpfs mounted over
whatever the image holds there.

`/etc/nftables.d/40-libvirt.nft` lets the NAT bridges (`virbr0` for the `default` network) forward
and reach their dnsmasq. libvirt's own table accepts the guests' traffic, but an accept ends only
the chain that issued it, and the forward and input chains of `/etc/nftables.conf` drop what they
were not told to pass. Without the file a guest boots, gets no DHCP lease and reaches nothing. The
rules match only `virbr*`, and the file belongs to this package, so it goes when libvirt does (see
[`/etc/nftables.conf`](../../../../docs/kdos/06-reference/configuration.md#etcnftablesconf)).

`virtlogd` holds every QEMU guest's console and log file open across a restart of `libvirtd`, and
the QEMU driver refuses to start a system-mode guest when it cannot reach virtlogd's socket, so
`62_virtlogd` comes before `63_libvirtd`. Both run without `--daemon`, so the supervisor watches
the daemon itself (see
[Supervision and logging](../../../../docs/kdos/04-programs/daemons.md#supervision-and-logging)).
