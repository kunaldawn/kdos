# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THREE GAMES OUT OF THIRTY-SEVEN, and the list is the point of this recipe.
#
# `configure` here is a hand-written interactive script, not autoconf: it asks
# thirty questions on a terminal and writes config.params. `non_interactive=y`
# plus a config.params written first is how it is driven from a build, and
# `build_dirs` is what stops the other thirty-four games being compiled, each
# with its own 1990s portability problem, for a package nobody asked for all of.
#
# HANGMAN IS NOT IN THE LIST, and that is measured rather than taste. It opens
# /usr/share/dict/words at startup and calls err(1) when it cannot — a hard
# exit, not a degraded game — and NOTHING ON THIS IMAGE PROVIDES A WORDLIST:
# there is no dictionary port, no /usr/share/dict, and bsd-games deliberately
# ships none of its own. A wordlist port existing for one game would be a
# megabyte of English on every image so that one program can start.
#
# libbsd IS A BUILD DEPENDENCY WITH NOTHING LINKED. Every source file includes
# bsd-games' own include/sys/cdefs.h, which is a `#include_next <sys/cdefs.h>`
# wrapper, so a system header of that name has to exist — and musl does not
# install one. On this image it comes from libbsd and from nothing else.
cat > config.params <<PARAMS
bsd_games_cfg_non_interactive=y
bsd_games_cfg_install_prefix="$PKG"
bsd_games_cfg_no_build_dirs=
bsd_games_cfg_build_dirs="adventure tetris worm"
bsd_games_cfg_gamesdir=/usr/games
bsd_games_cfg_man6dir=/usr/share/man/man6
bsd_games_cfg_sharedir=/usr/share/games
bsd_games_cfg_varlibdir=/var/games
bsd_games_cfg_do_chown=n
bsd_games_cfg_binary_perms=0755
bsd_games_cfg_score_game_perms=0755
bsd_games_cfg_manpage_perms=0644
bsd_games_cfg_constdata_perms=0644
bsd_games_cfg_vardata_perms=0666
bsd_games_cfg_vardata_perms_priv=0660
bsd_games_cfg_use_dot_so=.so
bsd_games_cfg_gzip_manpages=n
bsd_games_cfg_cc="$CC"
bsd_games_cfg_optimize_flags=
bsd_games_cfg_warning_flags=
bsd_games_cfg_other_cflags="$CFLAGS"
bsd_games_cfg_other_ldflags="$LDFLAGS"
bsd_games_cfg_ncurses_lib=-lncurses
bsd_games_cfg_ncurses_includes=
bsd_games_cfg_base_libs=
bsd_games_cfg_pager=/usr/bin/less
bsd_games_cfg_hangman_wordsfile=/usr/share/dict/words
bsd_games_cfg_tetris_scorefile=/var/games/tetris-bsd.scores
PARAMS

# other_cflags/other_ldflags AND NOT THE ENVIRONMENT. Makeconfig assigns
# `CFLAGS := $(OPTIMIZE) $(WARNING) @other_cflags@` with `:=`, which discards
# whatever was exported — so a phase's -ffile-prefix-map and its
# -Wl,--build-id=sha1 would be silently dropped and the package would stop
# hashing the same twice.
./configure
make
make install

# 0666 ON ONE FILE, AND THE ALTERNATIVE IS WORSE. tetris opens its score file
# O_RDWR|O_CREAT at the end of every game and calls err(1) when it cannot, so a
# root-owned 0644 file means the game dies the moment somebody loses. Upstream's
# own SECURITY file offers exactly two working policies: a world-writable score
# file, or a setgid binary in a package it says was not written with security in
# mind. This image has one setuid binary and means to keep it that way, so the
# score file is the one that carries no privilege. Nothing else in the package
# is writable, and the file holds scores.

install -d "$PKG/usr/share/applications"

cat > "$PKG/usr/share/applications/bsd-adventure.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Colossal Cave Adventure
GenericName=Text Adventure
Comment=Explore Colossal Cave and carry the treasure out
Exec=adventure
Icon=input-gaming
Terminal=true
Categories=Game;AdventureGame;
Keywords=adventure;text;cave;interactive;fiction;colossal;
ENTRY

cat > "$PKG/usr/share/applications/bsd-tetris.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Tetris
GenericName=Falling Blocks Game
Comment=Stack the falling shapes and clear the rows
Exec=tetris-bsd
Icon=input-gaming
Terminal=true
Categories=Game;BlocksGame;
Keywords=tetris;blocks;falling;puzzle;arcade;
ENTRY

cat > "$PKG/usr/share/applications/bsd-worm.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Worm
GenericName=Snake Game
Comment=Grow the worm without running into anything
Exec=worm
Icon=input-gaming
Terminal=true
Categories=Game;ArcadeGame;
Keywords=worm;snake;arcade;grow;
ENTRY

chmod 644 "$PKG"/usr/share/applications/bsd-*.desktop
