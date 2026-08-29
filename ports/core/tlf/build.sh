# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE LOG IS LOCAL AND THAT IS THE HONEST HALF. A contest logger's DX cluster
# feed is an internet service and is simply absent here; what works offline is
# the whole of the rest — dupe checking, multiplier tracking, the rig control
# through hamlib, and a Cabrillo file at the end that is the actual deliverable.
# A RULE-7 PATCH, two hunks and no flag for either:
#
#   FILPATHLEN -> HAMLIB_FILPATHLEN. hamlib 4 prefixed its public macros and
#   ports/core/hamlib is 4.7.2; tlf 1.4.1 is the newest release and predates
#   the rename, so the name it uses simply does not exist.
#
#   <sys/select.h> in sockserv.c, for FD_SETSIZE. glibc's <sys/socket.h> drags
#   it in and musl's does not, which is the usual shape of this one.
patch -p1 -i "$PORT_SRC/hamlib4-and-musl.patch"

# -largp because ARGP IS A GLIBC EXTENSION and musl does not implement it;
# argp-standalone is the port that provides it.
#
# LIBS, NOT LDFLAGS. automake's link rule expands $(LDFLAGS) BEFORE the object
# files and $(LIBS) after, and a static archive named before the objects that
# need it resolves nothing — the link still fails on `undefined reference to
# argp_parse` with the library right there on the command line. configure reads
# LIBS for its own probes too, so both halves see it.
export LIBS="${LIBS:-} -largp"

# The tag archive carries configure.ac and no configure.
autoreconf -fi
./configure --prefix=/usr --sysconfdir=/etc
make
make DESTDIR=$PKG install
