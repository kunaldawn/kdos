# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# iers.conf.auto_download IS THE ONE THING THAT MUST BE OFF, and it is not set
# here because it is a RUNTIME setting rather than a build one: astropy fetches
# `finals2000A.all` from the IERS the first time a precise time conversion
# needs it, and on a machine with no network that is a request that hangs
# rather than fails. astropy-iers-data ships a frozen copy and astropy falls
# back to it; a user who wants the fetch never to be attempted sets
# `iers.conf.auto_download = False` in their own astropy config.
#
# --no-build-isolation because every build dependency is an installed port;
# --no-deps because the runtime ones are too.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
