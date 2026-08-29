# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# EVERY VARIABLE IS PASSED ON THE make COMMAND LINE, and that is deliberate:
# upstream's `config` file sets LUA_VERSION=5.1 and a `make VAR=` argument
# beats both the environment and an assignment inside the makefile, which an
# exported variable would not.
make LUA_VERSION=$_lv \
     LUA_INC="-I/usr/include/lua$_lv" \
     CFLAGS="$CFLAGS -fPIC -I/usr/include/lua$_lv"
make DESTDIR=$PKG PREFIX=/usr LUA_VERSION=$_lv install
