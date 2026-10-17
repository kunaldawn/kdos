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

patch -p1 -i $PORT_SRC/openssl4-asn1-string-length.patch
patch -p1 -i $PORT_SRC/eventlog-mailer-gid.patch

./configure \
    --prefix=/usr \
        --libexecdir=/usr/lib \
        --with-secure-path \
        --with-all-insults \
        --with-env-editor \
        --with-editor=/usr/bin/nvim:/usr/bin/nano \
        --with-rundir=/run/sudo \
        --with-vardir=/var/lib/sudo \
        --with-passprompt="[sudo] password for %p: " \
        --with-pam \
        --enable-zlib=system \
        --enable-openssl \
        --without-sendmail
make
make DESTDIR=$PKG install
rm -rf $PKG/run

mkdir -m 755 $PKG/etc/pam.d
cat > $PKG/etc/pam.d/sudo << "EOF"
# Begin /etc/pam.d/sudo

# an enrolled finger, when there is a reader and one is on file; the leading
# dash skips the line on an image without fprintd rather than failing it
-auth     sufficient  pam_fprintd.so

# include the default auth settings
auth      include     system-auth

# include the default account settings
account   include     system-account

# Set default environment variables for the service user
session   required    pam_env.so

# include system session defaults
session   include     system-session

# End /etc/pam.d/sudo
EOF
chmod 644 $PKG/etc/pam.d/sudo
cat > $PKG/etc/sudoers.d/00-sudo << "EOF"
Defaults secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin"
Defaults env_keep += "EDITOR VISUAL SUDO_EDITOR"
%wheel ALL=(ALL) ALL
EOF
chmod 440 $PKG/etc/sudoers.d/00-sudo
