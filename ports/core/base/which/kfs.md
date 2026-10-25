## About
GNU which prints the full path of the program a command name would run, searching `$PATH` in
order. It is part of the userland every `build.sh` runs in: configure scripts and build systems
call `which` to find tools, and on KDOS that is always this program, so `30_foundation` installs it
in its first run, ahead of every shelf. Its build is upstream's default configure with nothing
changed.
