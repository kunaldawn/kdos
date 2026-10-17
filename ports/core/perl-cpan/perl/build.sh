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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/liblist-default-dirs.patch"

export BUILD_ZLIB=False BUILD_BZIP2=0

_kver=$(awk '$2 == "LINUX_VERSION_CODE" { c = $3; printf "%d.%d.%d", int(c / 65536), int(c / 256) % 256, c % 256 }' /usr/include/linux/version.h)
_time=$(LC_ALL=C date -u -d "@$SOURCE_DATE_EPOCH")
cat > config.over <<EOF
myhostname='localhost'
mydomain='.localdomain'
cf_email='root@localhost'
perladmin='root@localhost'
cf_by='kdos'
osvers='$_kver'
myuname='linux localhost $_kver x86_64'
cf_time='$_time'
EOF

./Configure -des \
	-Dprefix=/usr \
	-Dvendorprefix=/usr \
	-Dprivlib=/usr/share/perl5/core_perl \
	-Darchlib=/usr/lib/perl5/core_perl \
	-Dsitelib=/usr/share/perl5/site_perl \
	-Dsitearch=/usr/lib/perl5/site_perl \
	-Dvendorlib=/usr/share/perl5/vendor_perl \
	-Dvendorarch=/usr/lib/perl5/vendor_perl \
	-Dman1dir=/usr/share/man/man1 \
	-Dman3dir=/usr/share/man/man3 \
	-Dman3ext=3pm \
	-Doptimize="$CFLAGS" \
	-Dldflags="$LDFLAGS" \
	-Dlddlflags="-shared $LDFLAGS" \
	-Duseshrplib \
	-Dusethreads \
	-Dloclibpth=' ' \
	-Dlocincpth=' ' \
	-Dglibpth='/usr/lib' \
	-Ui_db \
	-Ud_eaccess -Ud_euidaccess
make
make DESTDIR=$PKG install
ln -s perl5/core_perl/CORE/libperl.so $PKG/usr/lib/libperl.so
