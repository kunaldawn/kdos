# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/adventure-linkdata-bounds.patch"

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

./configure
make
make install

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
