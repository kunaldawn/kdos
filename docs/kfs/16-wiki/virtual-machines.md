# Virtual machines

This page covers running [virtual machines]{.idx term="virtual machine"} on a KDOS machine: checking that the processor's
virtualisation reaches QEMU, starting a guest with `qemu-system-x86_64` from a prompt, managing
guests through libvirt and Virtual Machine Manager, giving a guest UEFI firmware, a [TPM]{.idx} and a
shared folder, and what changes when KDOS itself is the guest. How the services start is explained
in [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts)
and who may ask polkit for what in the
[security model](../../kdos/03-architecture/security-model.md); this page gives the procedures.

## Installation

Hardware virtualisation is part of the kernel and of the base system:

- The kernel is configured with `CONFIG_KVM`, `CONFIG_KVM_INTEL` and `CONFIG_KVM_AMD` as modules,
  and with `vhost-net`, `vhost-vsock`, `tun` and the bridge driver, in
  `ports/core/boot/linux/kdos.config`. See [linux](#port-linux).
- [eudev](#port-eudev)'s own rules, in `/usr/lib/udev/rules.d/50-udev-default.rules`, make
  `/dev/kvm`, [`/dev/vhost-net`]{.idx} and `/dev/vhost-vsock` owned by the `kvm` group with mode `0666`,
  so every account can open them; membership of `kvm` is not needed to run a guest. `01_udev`
  creates `/dev/vhost-net` before `udevd` starts, so the node exists before its module is loaded.

The emulator and the management stack are ports:

| Port | Provides |
|---|---|
| [qemu](#port-qemu) | `qemu-system-x86_64`, `qemu-system-aarch64` and `qemu-system-riscv64` (the three targets the recipe builds), `qemu-img`, `qemu-nbd` and the other tools, and the guest firmware under [`/usr/share/qemu`]{.idx} |
| [libvirt](#port-libvirt) | `libvirtd`, `virtlogd` and `virsh`, and the [service scripts]{.idx term="service script"} `62_virtlogd` and `63_libvirtd` |
| [virt-manager](#port-virt-manager) | Virtual Machine Manager, with `virt-install`, `virt-clone` and `virt-xml` |
| [swtpm](#port-swtpm) | The software TPM that a guest's emulated TPM runs on |
| [virtiofsd](#port-virtiofsd) | The daemon that shares a host directory with a guest over virtio-fs |
| [passt](#port-passt) | User-mode networking with no tap device and no privilege |
| [spice-gtk](#port-spice-gtk), [gtk-vnc](#port-gtk-vnc) | The SPICE and VNC console widgets Virtual Machine Manager draws a guest's screen with |
| [libosinfo](#port-libosinfo), [osinfo-db](#port-osinfo-db) | The operating-system facts `virt-install` and Virtual Machine Manager choose defaults from |

The libvirt package brings its two service scripts with it, and the image ships no
`/etc/service.disabled` marker for either, so `virtlogd` and `libvirtd` start at every boot while
the package is installed. Removing the package removes the scripts. `kpkg info libvirt` says
whether it is installed. On a booted live medium `service status` shows both `[ OK ]`.

The command-line procedures on this page have been run on the live medium, inside a KVM guest
with nested virtualisation: the KVM checks, a UEFI guest started with `qemu-system-x86_64`, passt,
`virsh` against `qemu:///system` as a member of `wheel`, the `default` network and its firewall
fragment. The Virtual Machine Manager window, an emulated TPM and a virtio-fs share are written
from the recipes and from upstream's documentation, and have not been opened on a KDOS image (see
[Known gaps](#known-gaps)). The live medium and an installed system carry the same packages; a
guest's disk image written on a live session is on the boot overlay and is gone at power-off
unless it is written to a mounted disk.

## Configuration

### The service scripts

The libvirt recipe writes both scripts as heredocs. They run their daemon under `ksvc supervise`,
in the foreground, so the supervisor restarts it after a crash:

<!-- kfs:include ports/core/virt/libvirt/build.sh lines=143-161 pin=66465bf63e11 -->

`62_virtlogd` has the same shape. Each gives its daemon a pid file under `/run/libvirt`, because
run as root the daemon's default, `/run/<name>.pid`, is the supervisor's own pid file. `-Dremote_default_mode=legacy` makes clients connect to the
single `libvirtd`, which serves `qemu:///system` and the other drivers; the recipe writes no
service script for upstream's per-driver daemons.

### Who may manage system guests

A connection to `qemu:///system` asks polkit for `org.libvirt.unix.manage`. The KDOS rules file
answers yes, with no prompt, to any member of `wheel`:

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=43-46 pin=b413b21a98a8 -->

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=133-149 pin=288b5dd7b9b9 -->

libvirt installs its own rule, `/usr/share/polkit-1/rules.d/50-libvirt.rules`, which grants the
same action to members of the [`libvirt`]{.idx} group. The package creates that group and the [`qemu`]{.idx}
account its guests run as, a member of `kvm`:

<!-- kfs:include ports/core/virt/libvirt/postinstall.sh lines=12-15 pin=134700f1675e -->

Why there is no polkit authentication agent, so that a rule is the only way to grant this, is in
[polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent).

### The firewall

The libvirt package adds a fragment to the firewall. It forwards the traffic of libvirt's NAT
bridges (`virbr0` and the rest) and lets guests reach the network's [`dnsmasq`]{.idx} for DHCP and DNS:

<!-- kfs:include ports/core/virt/libvirt/build.sh lines=107-118 pin=3abe04949ded -->

It is installed as [`/etc/nftables.d/40-libvirt.nft`]{.idx} and loaded with the rest of the ruleset; see
[The firewall](../../kdos/02-user-guide/administration.md#the-firewall).

### libvirt's own configuration

`libvirtd.conf`, `qemu.conf` and the other files under [`/etc/libvirt`]{.idx} are upstream's, installed
with every setting commented out at its default. The recipe sets the defaults that matter here at
build time: guests run as the `qemu` account in the `kvm` group, the firewall back end is nftables,
and there is no SELinux or AppArmor driver. Upstream also installs the `default` network,
[`/etc/libvirt/qemu/networks/default.xml`]{.idx}, marked to start with the daemon: a NAT bridge `virbr0` on
`192.168.122.1/24`, handing out addresses from `.2` to `.254`.

### Guest firmware

The recipe compiles SeaBIOS, iPXE, EDK2 for x86_64 and aarch64, and OpenSBI from the sources in
QEMU's tarball, and installs them in `/usr/share/qemu`. The riscv64 UEFI images are the prebuilt
ones the tarball carries, decompressed. Four firmware descriptors are written to
`/usr/share/qemu/firmware`, which is where libvirt looks when a guest asks for UEFI:

<!-- kfs:include ports/core/virt/qemu/build.sh lines=257-260 pin=7dbd1bdc39d5 -->

| Descriptor | Firmware | Machines |
|---|---|---|
| `60-edk2-x86_64.json` | `edk2-x86_64-code.fd`, variables from `edk2-i386-vars.fd` | `pc-i440fx-*`, `pc-q35-*` |
| `50-edk2-x86_64-secure.json` | `edk2-x86_64-secure-code.fd` ([Secure Boot]{.idx}, needs SMM), variables from `edk2-i386-vars.fd` | `pc-q35-*` |
| `60-edk2-aarch64.json` | `edk2-aarch64-code.fd`, `edk2-arm-vars.fd` | aarch64 `virt` |
| `60-edk2-riscv64.json` | `edk2-riscv-code.fd`, `edk2-riscv-vars.fd` | riscv64 `virt` |

The Secure Boot descriptor has no `enrolled-keys` feature, and its variable template is the same
`edk2-i386-vars.fd` as the plain build's, so a Secure Boot guest starts with no keys enrolled.

### Shared folders

virtiofsd's recipe writes the descriptor libvirt reads to find the daemon:

<!-- kfs:include ports/core/virt/virtiofsd/build.sh lines=18-31 pin=111562c7b602 -->

### Virtual Machine Manager

The recipe makes VNC the default console for a new guest and QEMU the default hypervisor:

<!-- kfs:include ports/core/virt/virt-manager/build.sh lines=12-18 pin=0483d8dbc89d -->

## Usage

### Checking that KVM is available

1. Check that the processor offers hardware virtualisation (`vmx` is Intel's flag, `svm` AMD's);
   a count of zero means it is off in the firmware set-up or absent:

   ```bash
   grep -cE 'vmx|svm' /proc/cpuinfo
   ```

2. Check that the module is loaded and the device node is there:

   ```bash
   lsmod | grep kvm
   ls -l /dev/kvm
   ```

   With no `kvm_intel` or `kvm_amd` line, load the one for your processor:

   ```bash
   sudo modprobe kvm_intel
   ```

3. Check that the node is open to you. `ls -l /dev/kvm` should show `crw-rw-rw-` and the group
   `kvm`; any account can then use it. On an AMD machine:

   ```text
   kvm_amd               266240  0
   kvm                  1470464  1 kvm_amd
   crw-rw-rw- 1 root kvm 10, 232 … /dev/kvm
   ```

   `/dev/vhost-net` and `/dev/vhost-vsock` show the same owner and mode.

### Running a guest with QEMU directly

This needs no daemon and no root. The example boots an installer image on UEFI with a virtio disk,
virtio graphics in a window, and user-mode networking.

1. Make a disk image and a private copy of the UEFI variable store:

   ```bash
   mkdir -p ~/vm/test && cd ~/vm/test
   qemu-img create -f qcow2 disk.qcow2 32G
   cp /usr/share/qemu/edk2-i386-vars.fd vars.fd
   ```

2. Boot it:

   ```bash
   qemu-system-x86_64 -enable-kvm -cpu host -smp 4 -m 4G \
     -drive if=pflash,format=raw,readonly=on,file=/usr/share/qemu/edk2-x86_64-code.fd \
     -drive if=pflash,format=raw,file=vars.fd \
     -drive file=disk.qcow2,if=virtio,format=qcow2 \
     -cdrom installer.iso \
     -device virtio-vga -display gtk \
     -nic user,model=virtio-net-pci
   ```

   `-nic user` is QEMU's built-in user-mode network. For passt instead, replace it with
   `-netdev passt,id=n0 -device virtio-net-pci,netdev=n0`. Neither needs privilege. passt prints
   `No IPv6 nameserver available for NDP/DHCPv6` on a host with no IPv6 resolver, and runs. With
   an empty disk and no `-cdrom`, the firmware tries the disk, then
   `>>Start PXE over IPv4.`, which is the sign that the UEFI images and KVM both work.

3. After the install, drop the `-cdrom` line and boot again from the disk.

Every option is described in `qemu-system-x86_64 -help` and in QEMU's own documentation. The
recipe builds without QEMU's documentation, so there is no `qemu(1)` page on the machine:
`man qemu` answers `No entry for qemu in the manual.` `qemu-system-x86_64 -display help` lists
the display back ends built: `none`, `gtk`, `sdl`, `egl-headless`, `curses`, `spice-app` and
`dbus`.

### Managing guests with Virtual Machine Manager

1. Check that both daemons are running, and start them if they are not:

   ```bash
   service status virtlogd
   service status libvirtd
   sudo service start virtlogd
   sudo service start libvirtd
   ```

2. Open **Virtual Machine Manager** from the Start menu, or run `virt-manager`. On its first run
   it connects to `qemu:///system`. A member of `wheel` is let in by the rule above with no password prompt; any
   other account must be in `libvirt`:

   ```bash
   sudo usermod -aG libvirt alice
   ```

3. Create a guest with **File → New Virtual Machine**. Its console opens in VNC unless you choose
   SPICE under the guest's display settings.

The same guests from a prompt:

```bash
virsh -c qemu:///system list --all
virsh -c qemu:///system start <guest>
virsh -c qemu:///system shutdown <guest>
```

Run as the first account, a member of `wheel`, `virsh -c qemu:///system list --all` answers with
the empty table and no prompt on a fresh machine.

### The default NAT network

1. List the networks and see whether `default` is active:

   ```bash
   virsh -c qemu:///system net-list --all
   ```

   As shipped it is already up, and `virbr0` holds `192.168.122.1/24`:

   ```text
    Name      State    Autostart   Persistent
   --------------------------------------------
    default   active   yes         yes
   ```

2. Start it, and mark it to start with the daemon if it is not. On an active network `net-start`
   answers `Requested operation is not valid: network is already active`:

   ```bash
   virsh -c qemu:///system net-start default
   virsh -c qemu:///system net-autostart default
   ```

3. Check that the firewall fragment is loaded, which is what lets the guests out. The line it
   prints is `iifname "virbr*" accept`:

   ```bash
   sudo nft list chain inet filter forward | grep virbr
   ```

   The network's own `dnsmasq`, two processes started with
   `--conf-file=/var/lib/libvirt/dnsmasq/default.conf`, runs beside NetworkManager's.

### A TPM for a guest

In the guest's details, **Add Hardware → TPM**, with the *Emulated* type. libvirt starts one
`swtpm` per guest, as the `tss` account, keeping its state under `/var/lib/libvirt/swtpm`. The
swtpm package's install script gives [`/var/lib/swtpm-localca`]{.idx} to `tss` (owner `tss`, group
`root`, mode `0750` on a booted image), which `swtpm_setup` needs to create the TPM's
certificates.

### A shared folder over virtio-fs

1. In the guest's **Memory** settings, enable shared memory; virtio-fs needs it.
2. **Add Hardware → Filesystem**, driver *virtiofs*, with the host directory as the source and a
   tag of your choosing as the target.
3. In a Linux guest, mount it by its tag:

   ```bash
   sudo mount -t virtiofs <tag> /mnt
   ```

### Running KDOS as a guest

A KDOS image runs under QEMU on any host with [KVM]{.idx}. The build tree's `make run` target is the
command line KDOS is developed against; it is in the repository's `Makefile`:

<!-- kfs:include Makefile lines=203-208 pin=a0d042b20a1c lang=make -->

That target runs on the build machine and expects its OVMF at `/usr/share/ovmf/OVMF.fd`; on a KDOS
host, use the firmware paths from [Running a guest with QEMU directly](#running-a-guest-with-qemu-directly).
What the guest does differently from real hardware:

- On a virtio GPU, `kdos-desktop` turns the hardware cursor off:

  <!-- kfs:include fs/usr/local/bin/kdos-desktop lines=13-17 pin=f01a3a00bfb4 -->

  and when the virtio GPU has no virgl (3D) support it puts the compositor on software rendering,
  where the phosphor pass is off:

  <!-- kfs:include fs/usr/local/bin/kdos-desktop lines=37-39 pin=ce1eec971951 -->

- A display device with no render node puts the compositor on software rendering too, with the
  phosphor pass off: QEMU's standard VGA (`-vga std`) and QXL, VirtualBox's VBoxVGA, and
  Hyper-V's synthetic video:

  <!-- kfs:include fs/usr/local/bin/kdos-desktop lines=44-51 pin=da56a7c7b973 -->

  A virtual GPU that has a render node and no 3D, VMware's SVGA or VirtualBox's VBoxSVGA with 3D
  acceleration off, is not caught here. When wlroots refuses its EGL device as software,
  `kdos-desktop-start` runs the compositor once more on the software renderer; when Mesa falls
  back to `kms_swrast` instead, the compositor runs in GLES2 on `llvmpipe` and declines only the
  phosphor pass; see
  [What `kdos-desktop-start` does](../../kdos/03-architecture/session.md#what-kdos-desktop-start-does).

- The compositor's idle dim, lock and screen-off timers and the lid action default to off; see
  [In a virtual machine](../../kdos/04-programs/kdos-comp.md#in-a-virtual-machine).
- PipeWire runs a larger quantum under a hypervisor:

  <!-- kfs:include fs/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf lines=38-48 pin=3d3c189a4c9e -->

  Why an emulated sound card needs it is in
  [The quantum, and why a virtual machine gets a bigger one](../../kdos/03-architecture/session.md#the-quantum-and-why-a-virtual-machine-gets-a-bigger-one).
- QEMU's emulated USB tablet is handled as a plain mouse:

  <!-- kfs:include fs/etc/udev/rules.d/99-qemu-tablet.rules -->

- `smartd` finds no disk that answers SMART and stops: `service status smartd` shows `[DOWN]`.
  `kdos doctor`'s Hardware section reports the wireless regulatory check and the attached-device
  check as `skip`, for want of a radio and a device.

## Tips

- On its first run Virtual Machine Manager adds `qemu:///system` and connects to it at every start,
  because it finds `qemu-system-*` in `/usr/bin`. The per-user `qemu:///session`, whose guests run
  as you and need no daemon at boot, is added with **File → Add Connection**; its guests are not
  in the system connection's list.
- A machine that never runs guests can stop the two daemons at boot with
  `sudo service disable libvirtd` and `sudo service disable virtlogd`; see [Services](services.md).
- `qemu-img convert -O qcow2 disk.vmdk disk.qcow2` brings in a disk from another hypervisor; the
  recipe enables VMDK, VHDX, VDI and VPC among other formats (the `--enable-` lines of
  `ports/core/virt/qemu/build.sh`).
- Use `-display sdl` in place of `-display gtk` for a window with no menu bar; both are built.
- `kdos why /etc/nftables.d/40-libvirt.nft` names the package that owns the fragment.

## Troubleshooting

**QEMU prints "Could not access KVM kernel module" or "failed to initialize kvm".** The module is
not loaded, or the processor's virtualisation is off in the firmware set-up. Work through
[Checking that KVM is available](#checking-that-kvm-is-available). A KDOS that is itself a guest
needs nested virtualisation from its host.

**Virtual Machine Manager says it cannot connect to libvirt.** The daemon is down or refused you.

1. `service status libvirtd`. If it is down, start it and read what it printed:

   ```bash
   sudo service start libvirtd
   sudo cat /run/kdos-svc.libvirtd.log
   ```

   The supervisor keeps the daemon's output in that file, readable by root only, and in the
   system log; `service start` itself prints only the script's own lines.

2. If it is up, check your groups with `id`: the account needs `wheel` or `libvirt`.

**A guest has no network.** Check that `default` is active
(`virsh -c qemu:///system net-list --all`; without `-c`, `virsh` run as an ordinary account
lists the per-user `qemu:///session` networks, of which there are none), that a
`dnsmasq` is running for it (`pgrep -a dnsmasq`), and that the firewall loaded the libvirt
fragment. When `/etc/nftables.conf` with its drop-ins does not load at boot, the `nftables`
service loads it without `/etc/nftables.d` and reports `[FAIL]`, and the libvirt fragment is then
missing. Checking the file prints the error without changing what is loaded:

```bash
sudo nft -c -f /etc/nftables.conf
```

See [Firewall](firewall.md).

**A guest with an emulated TPM does not start.** Check that `/var/lib/swtpm-localca` belongs to
`tss` with mode `0750`:

```bash
ls -ld /var/lib/swtpm-localca
```

If it does not, give it back, as the swtpm package's install script does:

```bash
sudo chown -R tss:root /var/lib/swtpm-localca
sudo chmod 0750 /var/lib/swtpm-localca
```

**A guest's log.** `virtlogd` keeps each guest's console and QEMU output in
`/var/log/libvirt/qemu/<guest>.log`. The directory is the package's, and the image carries it
empty, so before the first guest it holds nothing.

**QEMU stops with "Could not find ROM image".** The machine type needs firmware the recipe does not
build; see the first item under [Known gaps](#known-gaps).

**`-netdev bridge` fails as an ordinary user.** QEMU prints
`failed to parse default acl file '/etc/qemu/bridge.conf'` and `bridge helper failed`:
`qemu-bridge-helper` is installed without its setuid bit and no `bridge.conf` is shipped, so the
helper serves only root. That is a known gap; use user-mode networking, or a libvirt network.

## Known gaps

- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run)
- [qemu carries firmware only for its three targets' default machines](../../kdos/06-reference/known-gaps.md#qemu-carries-firmware-only-for-its-three-targets-default-machines)
- [A qemu guest cannot join a host bridge as an ordinary user](../../kdos/06-reference/known-gaps.md#a-qemu-guest-cannot-join-a-host-bridge-as-an-ordinary-user)
- [A SPICE viewer redirects only the USB devices you can already open](../../kdos/06-reference/known-gaps.md#a-spice-viewer-redirects-only-the-usb-devices-you-can-already-open)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)

## See also

- [Services](../../kdos/02-user-guide/administration.md#services): `62_virtlogd`, `63_libvirtd` and every other service script
- [The firewall](../../kdos/02-user-guide/administration.md#the-firewall): the ruleset the libvirt fragment joins
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): the groups an account is in, and what each grants
- [The security model](../../kdos/03-architecture/security-model.md): polkit with no agent, and the setuid list
- [Running the result](../../kdos/05-developer/developing.md#running-the-result): booting a KDOS image under QEMU on the build machine
- [Services](services.md), [Firewall](firewall.md), [Users and groups](users-and-groups.md),
  [sudo, PAM and polkit](sudo-pam-polkit.md), [Containers](containers.md), [Logs](logs.md),
  [Xwayland and X11 applications](xwayland-and-x11-apps.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 410.* Previous: [409. Containers](containers.md) · [Contents](../README.md) · Next: [411. Session and environment](session-and-environment.md)
