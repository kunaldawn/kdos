# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE BACKEND IS WHAT MAKES pyvisa USEFUL HERE. pyvisa is the API and talks to
# nothing on its own; NI-VISA is a proprietary blob that will never be on this
# machine, and pyvisa-py is the pure-python backend that speaks USBTMC, TCPIP
# and serial directly. python3-psutil is what lets list_resources enumerate
# TCPIP interfaces. The USBTMC and USB-raw sessions import pyusb, which is not
# a port, so they do not load and only TCPIP and serial answer; HiSLIP/VICP
# discovery needs zeroconf, which is not a port either.
#
# --no-build-isolation because setuptools and setuptools-scm are installed
# ports; --no-deps because everything else this needs is one too.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
