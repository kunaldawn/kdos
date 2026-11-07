#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   The boxes a session leaves running, recorded for the next login.
#
# `kdos-appbox session save` writes $XDG_STATE_HOME/kdos/session-boxes: every
# running box, and every boxed application that was open when the logout began
# (the launch ledgers the boxes' keepers keep). The autostart entry
# kdos-boxes-restore.desktop reads it at the next login, when ksmserverrc's
# loginMode is "restore the previous session". KDE's own restore cannot start
# a box; it starts launchers, and this record covers what it misses.
#
# IT RUNS BEFORE kdos.sh. plasma-shutdown runs one directory's files in name
# order, and `-` sorts before `.`, so the record is taken while the boxes that
# `kdos-box gc --session-end` is about to stop are still running. Renamed past
# kdos.sh, a box past its `autostop` would be stopped first and never come
# back.
#
# Bounded, as every shutdown script must be: a logout waits for it.
# ---------------------------------

command -v kdos-appbox >/dev/null 2>&1 || exit 0
timeout 10 kdos-appbox session save >&2
exit 0
