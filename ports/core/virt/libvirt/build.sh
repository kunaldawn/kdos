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

rm -rf "$PKG/usr/lib/sysusers.d"

rm -rf "$PKG/run"

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
