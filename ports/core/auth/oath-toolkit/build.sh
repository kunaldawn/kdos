# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# TWO HALVES ARE LEFT OUT, and what remains is exactly what a password store
# needs: liboath and `oathtool`.
#
# --disable-pam DROPS pam_oath, WHICH WOULD BE A SECOND WAY TO LOG IN. This
# image has PAM, so the module would build and could be wired into the login
# stack — and a PAM line that is wrong is a machine nobody can log into,
# including the person fixing it. A one-time password here is for a website
# that asks for one, not for this console.
#
# --disable-pskc DROPS THE XML KEY CONTAINER and libxml2 with it. PSKC is how
# an enterprise provisions ten thousand hardware tokens in one file; `pass otp`
# reads a single `otpauth://` URI out of an encrypted entry and needs none of
# it. The dependency is real — configure looks for libxml-2.0 and turns PSKC
# off by itself when it is missing, which is a decision made by what happens to
# be installed rather than by anybody.
./configure --prefix=/usr --disable-pskc --disable-pam --disable-static
make
make DESTDIR=$PKG install
