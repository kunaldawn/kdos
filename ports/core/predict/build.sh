# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A TLE IS PERISHABLE AND THAT IS THE HONEST LIMIT OF THIS PROGRAM. Orbital
# elements go stale in weeks, so offline this predicts passes accurately for a
# while after the last element set was copied onto the stick and drifts after.
# It is here anyway because nothing else on the machine can answer "when is it
# overhead" at all, and because the elements are a small file somebody can
# carry.
#
# UPSTREAM'S `build` SCRIPT IS NOT USED: it hardcodes -O3 and its own warning
# suppressions, and `configure` is an interactive prompt that asks where to
# install. The compile is one line.
cc $CFLAGS $LDFLAGS -Wno-deprecated-non-prototype -Wno-format-truncation \
   -Wno-unused-result predict.c -lncurses -pthread -lasound -lcurl -lm \
   -o predict \
   -DBUILD_DIR="\"/usr/share/predict/\"" \
   -DPREDICT_VERSION="\"$version\""

install -Dm755 predict $PKG/usr/bin/predict
install -Dm644 docs/man/predict.1 $PKG/usr/share/man/man1/predict.1
