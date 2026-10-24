# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cargo build --release --frozen --offline
install -Dm755 target/release/$name $PKG/usr/bin/$name
install -Dm644 packaging/man/taskwarrior-tui.1 -t "$PKG/usr/share/man/man1"
install -Dm644 completions/taskwarrior-tui.bash "$PKG/usr/share/bash-completion/completions/taskwarrior-tui"
install -Dm644 completions/_taskwarrior-tui -t "$PKG/usr/share/zsh/site-functions"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/taskwarrior-tui.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Tasks
GenericName=Task Manager
Comment=Taskwarrior's tasks, on a grid
Exec=taskwarrior-tui
Icon=x-office-document
Terminal=true
NoDisplay=true
Categories=Office;ProjectManagement;
Keywords=task;todo;project;taskwarrior;
EOF
chmod 644 "$PKG/usr/share/applications/taskwarrior-tui.desktop"
