# KDOS — where `plocate` looks for its index.
#
# THE INDEX IS THIS USER'S, IN THIS USER'S CACHE. plocate's compiled-in default
# is a shared /var/lib database that only a setgid binary can read; KDOS ships
# neither, so the path has to be said. The decision and what upstream does
# instead are in docs/kdos/03-architecture/security-model.md.
#
# LOCATE_PATH rather than an alias: `plocate` reads it itself, so the variable
# works from a script, from a pipeline and from any shell, and an alias would
# work in exactly one of the three.
export LOCATE_PATH="${XDG_CACHE_HOME:-$HOME/.cache}/kdos/plocate.db"
