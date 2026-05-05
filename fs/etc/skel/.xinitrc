#!/bin/sh
# KDOS xinitrc — minimal Window Maker session.
#
# Invoked by `startx` (xinit). The shell exits when wmaker exits.

# Load X resources
[ -f "$HOME/.Xresources" ] && xrdb -merge "$HOME/.Xresources"

# Sane keymap (override per machine if needed)
setxkbmap -layout us

# Disable bell. Disable screen blanking + DPMS at the X level
# (we read /sys/class/power_supply/ via wmbattery for power state).
xset b off
xset s off -dpms

# Notifications daemon
command -v dunst >/dev/null && dunst &

# Optional: a few NeXT-style dockapps. Window Maker swallows them
# into the Dock if they're listed in ~/.WindowMaker/autostart.
# (We let Window Maker handle that, not xinitrc.)

# Hand off to the WM
exec wmaker
