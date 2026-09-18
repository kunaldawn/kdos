# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLOSURE IS IN THE BUNDLE, not in thirteen more recipes. `pypackages`
# names khal's whole runtime set and `ports/fetch` crawls each tarball's own
# build-system.requires from there, so one bundle carries what a dozen ports
# would — the same shape python3-platformdirs and python3-meshtastic already
# use for the same reason.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF AND THE BACKENDS GO INTO THE BUILD ROOT, not into
# $PKG: a backend is what compiles a wheel, not something the image runs. With
# isolation on, pip would try to fetch them from a network this build does not
# have.
#
# THE ORDER IS A BOOTSTRAP. Each rung may use only backends the rungs above it
# have already installed — flit-core is self-hosting, setuptools-scm needs
# packaging, hatchling needs pathspec, pluggy and trove-classifiers, and
# hatch-vcs needs hatchling. Two rungs in one pip call fails with the LOWER one
# reported as a version that cannot be found, which reads like a missing file
# and is an ordering problem.
pyb() { pip3 install --no-index --find-links=vendor --no-build-isolation "$@"; }
pyb flit-core
pyb packaging pathspec calver
pyb trove-classifiers
pyb vcs-versioning
pyb setuptools-scm
pyb pluggy
pyb hatchling
pyb hatch-vcs

# --no-deps, because the bundle IS the dependency set and pip resolving it
# again would ask the network.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	click click-log configobj icalendar python-dateutil pytz pyxdg six \
	typing-extensions tzdata tzlocal urwid wcwidth .

# ikhal IS THE ENTRY POINT, and it is the one a menu row wants: `khal` prints
# and exits, `ikhal` is the full-screen calendar a person moves around in.
# Terminal=true and a bare Exec, so the launcher supplies the emulator this
# desktop uses rather than pinning one.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ikhal.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Calendar
GenericName=Calendar
Comment=Appointments, over a local vdir
Exec=ikhal
Icon=office-calendar
Terminal=true
Categories=Office;Calendar;
Keywords=calendar;appointment;event;ical;khal;
EOF
chmod 644 "$PKG/usr/share/applications/ikhal.desktop"
