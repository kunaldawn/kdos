#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Kolibri vendors its pure-Python dependencies (Django, Morango, CherryPy and
# the rest) under kolibri/dist, where it puts them first on sys.path; they are
# pinned to the versions it is tested with and ship inside it.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

# The archive also carries shared objects built elsewhere: cryptography for
# several platforms under dist/cext, and CPython 3.6 builds of greenlet,
# zoneinfo and SQLAlchemy's speedups that this interpreter cannot load. All are
# removed. Without dist/cext Kolibri imports the system cryptography; SQLAlchemy
# falls back to its pure-Python code, and greenlet serves only its asyncio
# layer, which Kolibri does not use.
site=$(echo "$PKG"/usr/lib/python3*/site-packages)
rm -rf "$site/kolibri/dist/cext"
find "$site/kolibri" -name '*.so' -delete
if find "$site/kolibri" -name '*.so*' | grep -q .; then
	echo 'a prebuilt shared object is left in the package' >&2
	exit 1
fi

# The statistics pingback to telemetry.learningequality.org, and the update
# notice it carries, run unless DISABLE_PING is set, so the command sets it
# before Kolibri reads its options. Kolibri lets the environment override
# options.ini, so only KOLIBRI_DISABLE_PING=false in the environment turns
# the pingback back on; the setting in options.ini is never reached.
cat > "$PKG/usr/bin/kolibri" <<'KDOS_SH'
#!/bin/sh
export KOLIBRI_DISABLE_PING="${KOLIBRI_DISABLE_PING:-true}"
exec python3 -m kolibri "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/kolibri"
