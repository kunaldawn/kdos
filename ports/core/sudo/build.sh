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

./configure \
    --prefix=/usr \
        --libexecdir=/usr/lib \
        --with-secure-path \
        --with-all-insults \
        --with-env-editor \
        --with-passprompt="[sudo] password for %p: "
make
make DESTDIR=$PKG install

mkdir -m 755 $PKG/etc/pam.d
cat > $PKG/etc/pam.d/sudo << "EOF"
# Begin /etc/pam.d/sudo

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
Defaults secure_path="/usr/sbin:/usr/bin"
%wheel ALL=(ALL) ALL
EOF
chmod 644 $PKG/etc/sudoers.d/00-sudo
