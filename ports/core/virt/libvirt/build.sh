#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# One driver set, named: QEMU/KVM, the NAT and bridge network driver
# (dnsmasq for DHCP and DNS, nftables for the rules), host interfaces through
# udev, secrets, node devices, the test driver virt-manager uses to open
# `test:///default`, and storage pools in directories, filesystems, disks,
# LVM, SCSI, multipath and iSCSI targets reached through libiscsi
# (iscsi-direct; the iscsiadm backend needs open-iscsi, which is not a port).
# nbdkit, through libnbd, serves network disks when a guest's XML or
# qemu.conf asks for it; QEMU's own block drivers stay the default. Every
# other hypervisor and storage backend is off: each one either needs a
# library that is not ported or is a feature nothing on this desktop uses,
# and an `auto` left to probe would link whichever happened to be installed
# first.
#
# -Dnss=disabled: libnss_libvirt is a glibc NSS module, and musl has no NSS
# to load it into.
# -Dnls=disabled: English only; the catalogues are the bulk of the install.
# -Dfirewall_backend_priority=nftables: the network driver's rules go into
# nft's own table beside /etc/nftables.conf. nwfilter still runs ebtables,
# iptables and ip6tables, the nft-backed commands of the iptables port.
# -Dqemu_user=qemu -Dqemu_group=kvm: system-mode guests run as their own
# account rather than root; postinstall.sh makes it, and the kvm group is
# what opens /dev/kvm. The DAC driver chowns each guest's disk images to it.
# -Dinit_script=none: the two init scripts below are KDOS's own.
# -Dremote_default_mode=legacy: clients connect to libvirtd's socket. The
# default, direct, makes them connect to virtqemud and the other modular
# daemons, which nothing here starts, so every qemu:///system connection
# would fail with the socket missing.
# -Dssh_proxy=disabled: it writes a client configuration under /etc/ssh.
# -Dsysctl_config=disabled: the postcopy-migration sysctl file is for hosts
# that migrate live guests.
# -Ddocs=enabled is what builds the manual pages (virsh(1) and the rest),
# through rst2man and xsltproc, and what generates /usr/share/libvirt/api/
# libvirt-api.xml: python3-libvirt generates its binding from that file and
# cannot build without it.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	--buildtype=release \
	-Drunstatedir=/run \
	-Dno_git=true \
	-Dgit_werror=disabled \
	-Drpath=disabled \
	-Dtests=disabled \
	-Dexpensive_tests=disabled \
	-Ddocs=enabled \
	-Dinit_script=none \
	-Dlibvirtd=enabled \
	-Ddriver_libvirtd=enabled \
	-Ddriver_remote=enabled \
	-Dremote_default_mode=legacy \
	-Ddriver_qemu=enabled \
	-Dqemu_user=qemu \
	-Dqemu_group=kvm \
	-Ddriver_network=enabled \
	-Ddriver_interface=enabled \
	-Ddriver_secrets=enabled \
	-Ddriver_test=enabled \
	-Ddriver_bhyve=disabled \
	-Ddriver_ch=disabled \
	-Ddriver_esx=disabled \
	-Ddriver_hyperv=disabled \
	-Ddriver_libxl=disabled \
	-Ddriver_lxc=disabled \
	-Ddriver_openvz=disabled \
	-Ddriver_vbox=disabled \
	-Ddriver_vmware=disabled \
	-Ddriver_vz=disabled \
	-Dstorage_dir=enabled \
	-Dstorage_fs=enabled \
	-Dstorage_disk=enabled \
	-Dstorage_lvm=enabled \
	-Dstorage_scsi=enabled \
	-Dstorage_mpath=enabled \
	-Dstorage_gluster=disabled \
	-Dstorage_iscsi=disabled \
	-Dstorage_iscsi_direct=enabled \
	-Dstorage_rbd=disabled \
	-Dstorage_vstorage=disabled \
	-Dstorage_zfs=disabled \
	-Dfirewalld=disabled \
	-Dfirewalld_zone=disabled \
	-Dfirewall_backend_priority=nftables \
	-Djson_c=enabled \
	-Dlibnl=enabled \
	-Dlibpcap=enabled \
	-Dpciaccess=enabled \
	-Dudev=enabled \
	-Dblkid=enabled \
	-Dattr=enabled \
	-Dcapng=enabled \
	-Dpolkit=enabled \
	-Dreadline=enabled \
	-Dsasl=enabled \
	-Dlibssh2=enabled \
	-Dlibssh=disabled \
	-Dcurl=disabled \
	-Dfuse=disabled \
	-Dglusterfs=disabled \
	-Dlibiscsi=enabled \
	-Dnetcf=disabled \
	-Dnumactl=disabled \
	-Dnumad=disabled \
	-Dopenwsman=disabled \
	-Dsanlock=disabled \
	-Dselinux=disabled \
	-Dsecdriver_selinux=disabled \
	-Dapparmor=disabled \
	-Dsecdriver_apparmor=disabled \
	-Dapparmor_profiles=disabled \
	-Daudit=disabled \
	-Ddtrace=disabled \
	-Dnbdkit=enabled \
	-Dnbdkit_config_default=disabled \
	-Dwireshark_dissector=disabled \
	-Dbash_completion=disabled \
	-Dhost_validate=enabled \
	-Dlogin_shell=disabled \
	-Dnss=disabled \
	-Dnls=disabled \
	-Dpm_utils=disabled \
	-Dssh_proxy=disabled \
	-Dsysctl_config=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# sysusers.d is systemd's account mechanism; postinstall.sh makes the
# accounts. The directory is installed whatever -Dinit_script says.
rm -rf "$PKG/usr/lib/sysusers.d"

# /run is a tmpfs mounted over whatever the image holds there, so the empty
# runstatedir tree meson installs would never be seen; each daemon creates
# its own under /run/libvirt at start.
rm -rf "$PKG/run"

# The NAT networks the network driver makes (virbr0 for `default`). libvirt
# adds its own table whose forward chain accepts the guests' traffic, but an
# accept ends only the chain that issued it, and the forward chain in
# /etc/nftables.conf drops everything it was not told to pass; its input
# chain drops too. Without these a guest boots, gets no DHCP lease from the
# network's dnsmasq and reaches nothing. Scoped to the bridge name, so this
# opens forwarding for guests and for nothing else; the connection coming
# back is the main forward chain's first rule. Owned by this package, so the
# opening goes when libvirt does.
install -d "$PKG/etc/nftables.d"
cat > "$PKG/etc/nftables.d/40-libvirt.nft" <<'NFT'
table inet filter {
	chain forward {
		iifname "virbr*" accept
	}

	chain input {
		iifname "virbr*" udp dport { 53, 67 } accept
		iifname "virbr*" tcp dport 53 accept
	}
}
NFT
chmod 644 "$PKG/etc/nftables.d/40-libvirt.nft"

# virtlogd holds every QEMU guest's console and log file open across a
# libvirtd restart, and the QEMU driver refuses to start a system-mode guest
# when it cannot reach virtlogd's socket; so it comes up first.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/62_virtlogd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="virtlogd"
DAEMON="/usr/sbin/virtlogd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        # No --daemon: ksvc must watch the daemon itself.
        supervise "$NAME" "$DAEMON"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/62_virtlogd.sh"

# After 40_dbus and 41_polkitd: libvirtd asks polkit whether a client may
# manage qemu:///system, and the rule it installs answers yes for the
# `libvirt` group. After 25_nftables, whose ruleset the network driver adds
# its own table beside.
cat > "$PKG/etc/init.d/63_libvirtd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="libvirtd"
DAEMON="/usr/sbin/libvirtd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        # No --daemon: ksvc must watch the daemon itself.
        supervise "$NAME" "$DAEMON"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/63_libvirtd.sh"
