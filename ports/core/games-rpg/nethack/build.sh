# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p lib
cp -R "$SRC_ROOT/lua-$_luaver" lib/

cat > sys/unix/hints/kdos <<HINTS
#-PRE
PREFIX=/usr
HACKDIR=\$(PREFIX)/share/$name
SHELLDIR=
INSTDIR=$PKG\$(HACKDIR)
VARDIR=\$(INSTDIR)

CFLAGS=$CFLAGS -D_GNU_SOURCE -I../include
CFLAGS+=-DDLB
CFLAGS+=-DZLIB_COMP
CFLAGS+=-DHACKDIR=\"\$(HACKDIR)\"
CFLAGS+=-DSYSCF -DSYSCF_FILE=\"/etc/$name/sysconf\"
CFLAGS+=-DTIMED_DELAY
CFLAGS+=-DDUMPLOG
CFLAGS+=-DCURSES_GRAPHICS
CFLAGS+=-DCURSES_UNICODE -D_XOPEN_SOURCE_EXTENDED
CFLAGS+=-DCONFIG_ERROR_SECURE=FALSE
CFLAGS+=-DNOCRASHREPORT
CFLAGS+=-DSELF_RECOVER

SYSCFLAGS=-DLUA_USE_POSIX $CFLAGS

LINK=\$(CC)
LFLAGS=$LDFLAGS

WINSRC = \$(WINTTYSRC) \$(WINCURSESSRC)
WINOBJ = \$(WINTTYOBJ) \$(WINCURSESOBJ)
WINLIB = -lncursesw

CHOWN=true
CHGRP=true
GAMEPERM = 0755
VARDIRPERM = 0755
VARFILEPERM = 0644
#-POST
LIBS = -lz
HINTS

sh sys/unix/setup.sh sys/unix/hints/kdos

make -C src LUA_VERSION=$_luaver hacklib.a
make LUA_VERSION=$_luaver install

install -Dm755 "$PKG/usr/share/$name/$name" "$PKG/usr/libexec/$name/$name"
install -Dm755 "$PKG/usr/share/$name/recover" "$PKG/usr/libexec/$name/recover"
rm -f "$PKG/usr/share/$name/$name" "$PKG/usr/share/$name/recover"

rm -f "$PKG/usr/share/$name"/perm "$PKG/usr/share/$name"/record \
      "$PKG/usr/share/$name"/logfile "$PKG/usr/share/$name"/xlogfile \
      "$PKG/usr/share/$name"/livelog
rmdir "$PKG/usr/share/$name/save"

install -Dm644 doc/$name.6 "$PKG/usr/share/man/man6/$name.6"
install -Dm644 doc/recover.6 "$PKG/usr/share/man/man6/$name-recover.6"

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
