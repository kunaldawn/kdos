# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE TPM A GUEST SEES. qemu's -tpmdev emulator talks to a swtpm process over a
# socket, and that is the only virtual TPM qemu has: the passthrough backend
# hands the guest the host's own chip. A Windows 11 installer refuses to run
# without one, and so does a guest whose disk key is sealed to its PCRs.
#
#   swtpm_setup --create-config-files skip-if-exist
#   swtpm_setup --tpm2 --tpmstate DIR --create-ek-cert --create-platform-cert
#   swtpm socket --tpm2 --tpmstate dir=DIR --ctrl type=unixio,path=DIR/sock
#   qemu-system-x86_64 -chardev socket,id=tpm,path=DIR/sock \
#       -tpmdev emulator,id=tpm0,chardev=tpm -device tpm-tis,tpmdev=tpm0 ...
#
# swtpm_setup signs the TPM's endorsement certificates through swtpm_localca,
# which runs gnutls's certtool. The shipped /etc/swtpm_setup.conf points it at
# the CA in /var/lib/swtpm-localca, which the package installs root-owned, so a
# user's first swtpm_setup with the certificate flags stops on "Need read/write
# rights on statedir". --create-config-files writes ~/.config/swtpm_setup.conf
# and a CA of the user's own under ~/.config/var/lib/swtpm-localca, and
# swtpm_setup reads that file ahead of /etc whenever it exists. Without the
# certificate flags no CA is involved; a Windows 11 guest does not need them. The CUSE interface is off: it creates a
# /dev/vtpm character device for a host program, which qemu does not use.
# SELinux is not on this system. The test suite installs itself under
# /usr/lib/installed-tests even with --disable-tests, and is removed.
autoreconf -fi
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--libexecdir=/usr/lib --disable-static \
	--with-openssl \
	--with-gnutls \
	--with-seccomp \
	--without-cuse \
	--without-selinux \
	--disable-tests
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/lib/installed-tests"
