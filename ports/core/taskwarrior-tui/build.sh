# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE FRONT END, NOT A SECOND TASK DATABASE. It shells out to the `task`
# binary the taskwarrior port installs and reads its JSON, so the two cannot
# disagree about what a task is; without it taskwarrior is a query language
# with no way to look at what it answered.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cargo build --release --frozen --offline
install -Dm755 target/release/$name $PKG/usr/bin/$name

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
Categories=Office;ProjectManagement;
Keywords=task;todo;project;taskwarrior;
EOF
chmod 644 "$PKG/usr/share/applications/taskwarrior-tui.desktop"
