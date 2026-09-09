# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# NetHack has no configure. sys/unix/setup.sh splices a hints file into each
# generated Makefile, so the hints file below IS this port's configuration.
# Assignments in its #-PRE segment land before the stock Makefile body and are
# beaten by anything the body assigns uncommented, which is why LIBS is set in
# the #-POST segment instead.

# Upstream's own pre-generated parser sources, so the port needs neither yacc
# nor lex. The copy makes them newer than the .y/.l they came from, which is
# what stops make from trying to regenerate them.
cp sys/share/lev_yacc.c sys/share/lev_lex.c \
   sys/share/dgn_yacc.c sys/share/dgn_lex.c util/
cp sys/share/lev_comp.h sys/share/dgn_comp.h include/

cat > sys/unix/hints/kdos <<HINTS
#-PRE
PREFIX=/usr
HACKDIR=\$(PREFIX)/share/$name
SHELLDIR=
INSTDIR=$PKG\$(HACKDIR)
VARDIR=\$(INSTDIR)

# -std=gnu17 BECAUSE include/system.h STILL DECLARES LIBC BY HAND. It carries
# unprototyped K&R declarations of srand48() and sleep() for the case where
# neither BSD nor RANDOM is defined, which is the case on Linux. Under C23 an
# empty parameter list means (void), so those collide with the real prototypes
# and every translation unit fails before the first line of NetHack compiles.
#
# -D_GNU_SOURCE BECAUSE ONE FILE BREAKS THE ORDER config1.h RELIES ON.
# config1.h defines _GNU_SOURCE itself, with the comment that it must happen
# before any system header; win/curses/cursmain.c includes <curses.h> on the
# line above hack.h, so the C library is configured without it and then asked
# for mempcpy() by a fortifying <string.h>. Setting it on the command line
# makes the order irrelevant.
CFLAGS=-O2 -std=gnu17 -D_GNU_SOURCE -I../include -DNOTPARMDECL
CFLAGS+=-DDLB
CFLAGS+=-DZLIB_COMP
CFLAGS+=-DHACKDIR=\"\$(HACKDIR)\"
CFLAGS+=-DSYSCF -DSYSCF_FILE=\"/etc/$name/sysconf\"
CFLAGS+=-DTIMED_DELAY
CFLAGS+=-DDUMPLOG
CFLAGS+=-DCURSES_GRAPHICS
CFLAGS+=-DCONFIG_ERROR_SECURE=FALSE

LINK=\$(CC)

WINSRC = \$(WINTTYSRC) \$(WINCURSESSRC)
WINOBJ = \$(WINTTYOBJ) \$(WINCURSESOBJ)
# -lncursesw alone: our ncurses is built wide-character and without
# --with-termlib, so there is no libtinfo to link and no narrow libncurses.
WINLIB = -lncursesw

# No setgid playground, so nothing to give away: the game runs as the player
# and every file it writes belongs to the player.
CHOWN=true
CHGRP=true
GAMEPERM = 0755
VARDIRPERM = 0755
VARFILEPERM = 0644
#-POST
LIBS = -lz
HINTS

sh sys/unix/setup.sh sys/unix/hints/kdos

# 'make all' would also build the Guidebook, which needs nroff, tbl and col.
# 'make install' builds the game, recover, the data files and the DLB archive
# and nothing else.
make install

# The binaries do not belong in a data directory. HACKDIR names where the read
# only data lives and nothing else, so the two executables move out and the
# wrappers below reach them by full path.
install -Dm755 "$PKG/usr/share/$name/$name" "$PKG/usr/libexec/$name/$name"
install -Dm755 "$PKG/usr/share/$name/recover" "$PKG/usr/libexec/$name/recover"
rm -f "$PKG/usr/share/$name/$name" "$PKG/usr/share/$name/recover"

# 'make install' seeds an empty score file, log and lock in VARDIR. They are
# per player state here and the game creates them on first run, so shipping
# them would put four files nobody may write in a read-only directory.
rm -f "$PKG/usr/share/$name"/perm "$PKG/usr/share/$name"/record \
      "$PKG/usr/share/$name"/logfile "$PKG/usr/share/$name"/xlogfile
rmdir "$PKG/usr/share/$name/save"

install -Dm644 doc/$name.6 "$PKG/usr/share/man/man6/$name.6"
install -Dm644 doc/recover.6 "$PKG/usr/share/man/man6/$name-recover.6"

# SYSCF IS NOT OPTIONAL: config.h turns it on unless something else already
# has, and a build with SYSCF refuses to start when it cannot read the file.
# The compiled-in path is absolute so that it resolves from the player's
# playground, which is where the process spends its whole life.
install -d "$PKG/etc/$name"
cat > "$PKG/etc/$name/sysconf" <<SYSCONF
# System-wide NetHack configuration. Per-player options go in ~/.nethackrc.

# Wizard and explore mode are open to everyone because there is nobody to
# protect them from: each player's playground is their own directory and the
# binary carries no privilege, so a debug game can only rewrite files its
# player already owns.
WIZARDS=*
EXPLORERS=*

# Named so that a dump lands in the player's own playground, which is the
# process's working directory. An absolute path here would be one directory
# every player writes to.
DUMPLOGFILE=dumplog.%n.%d.txt
SYSCONF
chmod 644 "$PKG/etc/$name/sysconf"

# THE PLAYGROUND IS PER PLAYER, AND THIS SCRIPT IS THE WHOLE OF THAT.
#
# NetHack keeps saves, bones, level files, locks and the score file in its
# working directory unless it was compiled with VAR_PLAYGROUND, and
# VAR_PLAYGROUND is a compile-time string constant that no runtime value can
# reach -- files.c expands '~' and '%VAR%' in a prefix only under WIN32. So
# the upstream shapes are a shared setgid directory or nothing, and the way to
# get a per-player playground is to give each player their own working
# directory: unixmain.c reads $NETHACKDIR and chdir()s there before it opens a
# single file.
#
# That directory therefore has to contain the read-only data too, since with
# no prefixes in use every name the game opens is relative to it. Three
# symlinks is what that costs. They are refreshed on every run rather than
# created once, so an upgraded nhdat is picked up instead of a stale link
# outliving the package that made it.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/$name" <<'WRAPPER'
#!/bin/sh
# Start NetHack with a playground under the player's own data directory.
set -e

NETHACKDIR="${XDG_DATA_HOME:-$HOME/.local/share}/nethack"
export NETHACKDIR

# save/ is not created on demand: set_savefile_name() builds "save/<uid><name>"
# and the open fails as a save error mid-game if the directory is missing.
mkdir -p "$NETHACKDIR/save"

# Every file the game takes an fcntl lock on has to exist first: lock_file()
# opens its target O_RDWR with no O_CREAT, so an absent one is not created but
# reported as "Cannot open file <name>.  Is NetHack installed correctly?".
# perm is the instance lock and stays empty; the other two are appended to at
# the end of each game. The score file is not in this list because
# check_recordfile() does create that one.
for f in perm logfile xlogfile; do
	[ -f "$NETHACKDIR/$f" ] || : > "$NETHACKDIR/$f"
done

for f in nhdat license symbols; do
	ln -sf "/usr/share/nethack/$f" "$NETHACKDIR/$f"
done

exec /usr/libexec/nethack/nethack "$@"
WRAPPER
chmod 755 "$PKG/usr/bin/$name"

# recover reads and rewrites the level files of an interrupted game, all named
# relative to the working directory, so it has to be run from the playground.
cat > "$PKG/usr/bin/$name-recover" <<'WRAPPER'
#!/bin/sh
# Rebuild a save file from the level files an interrupted game left behind.
set -e

cd "${XDG_DATA_HOME:-$HOME/.local/share}/nethack"
exec /usr/libexec/nethack/recover "$@"
WRAPPER
chmod 755 "$PKG/usr/bin/$name-recover"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/$name.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=NetHack
GenericName=Roguelike
Comment=Descend the Dungeons of Doom and recover the Amulet of Yendor
Exec=nethack
Icon=input-gaming
Terminal=true
Categories=Game;AdventureGame;RolePlaying;
Keywords=roguelike;dungeon;rpg;adventure;nethack;hack;
ENTRY
chmod 644 "$PKG/usr/share/applications/$name.desktop"
