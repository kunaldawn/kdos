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
# settings, so the CONSOLE client refuses to start without wxPython, which this
# port does not depend on because it ships no wx front end. No flag defers an
# import.
patch -p1 -i "$PORT_SRC/no-wx-on-console.patch"

# --no-deps because requirements.txt is the GUI's: wxPython, pyglet, numpy,
# lxml, puremagic and dbus-python are what pronterface needs and pronsole does
# not. Of the five things the console path really imports, serial,
# platformdirs and psutil are in `depends` (psutil is how pronsole raises its
# priority while printing); dbus, for the sleep inhibit through
# org.freedesktop.ScreenSaver, sits inside a try/except with a working
# fallback, and python3-dbus is not in `depends`, so a print does not hold
# off the idle lock. --no-build-isolation because the build
# dependencies, setuptools and cython, are ports.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

# pronterface and plater are the wx front ends and cannot run here: their 3D
# and projector views import pyglet and puremagic, which are not ports, so
# leaving their scripts installed would put two commands on the PATH that die
# on an import. They install with a .py suffix, which is what setup.py's `scripts`
# list names them.
rm -f $PKG/usr/bin/pronterface.py $PKG/usr/bin/plater.py
rm -f $PKG/usr/bin/__pycache__/pronterface.*.pyc $PKG/usr/bin/__pycache__/plater.*.pyc

# AND THEIR LAUNCHERS WITH THEM. A desktop entry is an entry in the Start menu,
# the launcher and the taskbar's search; one naming a command this recipe has
# just deleted is a row that opens nothing and says nothing about why.
rm -f $PKG/usr/share/applications/pronterface.desktop \
      $PKG/usr/share/applications/plater.desktop \
      $PKG/usr/share/metainfo/pronterface.appdata.xml \
      $PKG/usr/share/metainfo/plater.appdata.xml

# THE COMMAND IS `pronsole`, not `pronsole.py`. Upstream's scripts keep their
# suffix because setup.py lists the files rather than entry points, and every
# guide, every forum answer and the plan's own row call it by the bare name.
ln -s pronsole.py $PKG/usr/bin/pronsole
ln -s printcore.py $PKG/usr/bin/printcore
