# KDOS — where an mpd client finds the music daemon.
#
# THE SOCKET IN THE RUNTIME DIRECTORY, NOT TCP. The mpd user service
# (/etc/kdos/user-services/25_mpd.sh) makes $XDG_RUNTIME_DIR/mpd, and
# kdos-mpctl reaches mpd through $XDG_RUNTIME_DIR/mpd/socket alone; mpc, rmpc
# and ncmpcpp default to localhost:6600 and read MPD_HOST before that, so one
# variable points them all at the same daemon. One already in the environment
# is kept, and a user's own profile, read after this, can set another.
if [ -z "$MPD_HOST" ] && [ -n "$XDG_RUNTIME_DIR" ]; then
    export MPD_HOST="$XDG_RUNTIME_DIR/mpd/socket"
fi
