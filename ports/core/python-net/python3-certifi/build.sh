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

# A RUNTIME DEPENDENCY OF skyfield, AND A PORT RATHER THAN A VENDORED WHEEL.
# Every python recipe here installs --no-deps, so a vendored runtime set is
# never used; carrying these as ports is what makes `import skyfield` work on
# the target. Pure python, no build step beyond setuptools.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

# The file certifi.where() names is a link to the system bundle, so python
# trusts exactly the roots every other program here trusts and follows a
# ca-certificates rebuild with no certifi rebuild. The link is absolute and
# dangles until ca-certificates is installed, which is why that is a depend.
pem=$(echo "$PKG"/usr/lib/python3*/site-packages/certifi/cacert.pem)
test -f "$pem"
ln -sf /etc/ssl/cert.pem "$pem"
