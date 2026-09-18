# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A pass EXTENSION IS A FILE IN A DIRECTORY pass ALREADY READS. The shipped
# pass takes system extensions from /usr/lib/password-store/extensions with no
# opt-in — `PASSWORD_STORE_ENABLE_EXTENSIONS` gates only the ones in a user's
# own store — and this Makefile's SYSTEM_EXTENSION_DIR defaults to exactly
# that under PREFIX=/usr. So `pass otp` works the moment the package is
# installed, with nothing to enable.
#
# oathtool IS FOUND ON $PATH AT RUN TIME, not linked: otp.bash line 20 is
# `OATH=$(which oathtool)` and line 308 refuses with "oathtool is not
# installed" when it is empty. That is why oath-toolkit is a `depends` rather
# than something the recipe checks for.
make PREFIX=/usr DESTDIR=$PKG install
