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
# TCPIP interfaces. The USBTMC and USB-raw sessions import pyusb and are
# dropped without an error when that import fails, so python3-pyusb in depends
# is what keeps USB instruments in list_resources. They open the raw USB
# device, not /dev/usbtmc*, which is why 70-kdos-usbtmc.rules grants the
# usb_device node as well. HiSLIP/VICP discovery needs zeroconf, which is not
# a port.
#
# --no-build-isolation because setuptools and setuptools-scm are installed
# ports; --no-deps because everything else this needs is one too.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
