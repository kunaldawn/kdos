# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE STICK CAN SLICE AND HAD NO WAY TO SEND. PrusaSlicer and the rest turn a
# model into gcode; nothing on this machine could then push that gcode down a
# USB serial line to the printer. pronsole is that, and printcore is the
# library under it.
#
# A RULE-7 PATCH: settings.py imports wx at module scope and pronsole imports
# settings, so the CONSOLE client refuses to start without wxPython — which is
# a GUI toolkit this host does not have by rule. No flag defers an import.
patch -p1 -i "$PORT_SRC/no-wx-on-console.patch"

# --no-deps because requirements.txt is the GUI's: wxPython, pyglet, numpy,
# lxml and dbus-python are what pronterface needs and pronsole does not. Of the
# five things the console path really imports, dbus and psutil are already
# inside try/except with working fallbacks, and serial and platformdirs are
# ports. --no-build-isolation because the only build dependency is setuptools.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

# pronterface and plater are the wx front ends and cannot run here; leaving
# their scripts installed would put two commands on the PATH that die on an
# import. They install with a .py suffix, which is what setup.py's `scripts`
# list names them.
rm -f $PKG/usr/bin/pronterface.py $PKG/usr/bin/plater.py
rm -f $PKG/usr/bin/__pycache__/pronterface.*.pyc $PKG/usr/bin/__pycache__/plater.*.pyc

# THE COMMAND IS `pronsole`, not `pronsole.py`. Upstream's scripts keep their
# suffix because setup.py lists the files rather than entry points, and every
# guide, every forum answer and the plan's own row call it by the bare name.
ln -s pronsole.py $PKG/usr/bin/pronsole
ln -s printcore.py $PKG/usr/bin/printcore
