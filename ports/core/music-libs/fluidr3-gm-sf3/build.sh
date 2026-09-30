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


# The Vorbis-compressed mono FluidR3 set MuseScore carries in its tree: 23 MB
# against the 141 MB of the uncompressed stereo sf2, with the full General
# MIDI bank and drum kits. Loading it needs a libsndfile with Vorbis, which
# fluidsynth has here.
#
# default.sf2 is the file fluidsynth opens when it is started with no
# SoundFont argument, and the first path SDL3_mixer tries; SDL2_mixer tries
# only sounds/sf2/FluidR3_GM.sf2, so that name links here too or its MIDI
# music is silent. The loader reads the RIFF header rather than the
# extension, so a link with an sf2 name pointing at an sf3 loads. MuseScore
# ships its own MS Basic set and does not read this one.
install -Dm644 FluidR3Mono_GM-$version.sf3 "$PKG/usr/share/soundfonts/FluidR3Mono_GM.sf3"
ln -s FluidR3Mono_GM.sf3 "$PKG/usr/share/soundfonts/default.sf2"
install -d "$PKG/usr/share/sounds/sf2"
ln -s ../../soundfonts/FluidR3Mono_GM.sf3 "$PKG/usr/share/sounds/sf2/FluidR3_GM.sf2"
install -Dm644 FluidR3Mono_License-$version.md "$PKG/usr/share/licenses/$name/LICENSE.md"
