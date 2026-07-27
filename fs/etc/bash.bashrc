# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# /etc/bash.bashrc — interactive shell setup for every bash, not just login ones.
#
# /etc/profile is read by login shells only (tty, serial, ssh). A terminal like
# foot starts a NON-login interactive shell, which reads ~/.bashrc instead — so
# anything interactive (aliases, prompt) belongs here, and gets picked up by
# both: bash reads this file directly (SYS_BASHRC), ~/.bashrc sources it as a
# fallback, and /etc/profile sources it for login shells.

# Nothing here is useful to a script.
case $- in
    *i*) ;;
    *) return ;;
esac

# Colors
alias ls='ls --color=auto'
alias grep='grep --color=auto'

# Aliases
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'
alias cp='cp -i'
alias mv='mv -i'
alias rm='rm -i'

# Prompt. Inside a distrobox the box name is prefixed, so it is obvious which
# side of the container boundary a shell is on — this file is shared with every
# box through /run/host (see ~/.bashrc), so the same prompt follows you in.
if [ -n "${CONTAINER_ID:-}" ]; then
    PS1='\[\033[01;35m\]⬢ '"${CONTAINER_ID}"'\[\033[00m\] \[\033[01;32m\]\u\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\] λ '
else
    PS1='\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\] λ '
fi

# History
HISTSIZE=5000
HISTFILESIZE=10000
HISTCONTROL=ignoreboth
shopt -s histappend checkwinsize
