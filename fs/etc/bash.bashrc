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
#
# This file is also visible inside every distrobox through /run/host, so the
# same shell follows you into a container. Nothing here may ASSUME a KDOS
# userland: every tool is probed before it is used, because a Debian box has
# neither eza nor starship.

# Nothing here is useful to a script.
case $- in
    *i*) ;;
    *) return ;;
esac

# A login shell reads this file TWICE: once because /etc/profile sources it, and
# again because ~/.bash_profile sources ~/.bashrc which falls back to it. That is
# harmless for aliases and fatal for the banner — fastfetch printed itself twice
# on every terminal. Everything below runs once per shell.
[ -n "${__KDOS_BASHRC:-}" ] && return
__KDOS_BASHRC=1

# ── History ──────────────────────────────────────────────────────────
HISTSIZE=50000
HISTFILESIZE=100000
HISTCONTROL=ignoreboth:erasedups
HISTTIMEFORMAT='%F %T  '
HISTIGNORE='ls:ll:la:cd:pwd:exit:clear:history'
shopt -s histappend checkwinsize cmdhist
shopt -s autocd cdspell dirspell 2>/dev/null
# Write history as it happens, so a crashed session does not lose it and two
# terminals do not clobber each other.
PROMPT_COMMAND="history -a${PROMPT_COMMAND:+; $PROMPT_COMMAND}"

# ── Core aliases ─────────────────────────────────────────────────────
alias grep='grep --color=auto'
alias diff='diff --color=auto'
alias cp='cp -i'
alias mv='mv -i'
alias rm='rm -i'
alias mkdir='mkdir -p'
alias df='df -h'
alias du='du -h'
alias free='free -h'
alias path='printf "%s\n" ${PATH//:/ }'
alias ports='ls -1 /var/lib/kpkg/db 2>/dev/null | wc -l'
alias ..='cd ..'
alias ...='cd ../..'
alias ....='cd ../../..'

# ── Modern replacements, when they exist ─────────────────────────────
# KDOS ships all of these; a distrobox usually ships none. Probe, never assume.
if command -v eza >/dev/null 2>&1; then
    alias ls='eza --group-directories-first'
    alias ll='eza -l --group-directories-first --git --time-style=long-iso'
    alias la='eza -la --group-directories-first --git --time-style=long-iso'
    alias lt='eza --tree --level=2 --group-directories-first'
    alias l='eza -1'
else
    alias ls='ls --color=auto'
    alias ll='ls -alF'
    alias la='ls -A'
    alias l='ls -CF'
fi

if command -v bat >/dev/null 2>&1; then
    # cat only — `less` stays itself, because half the userland shells out to it
    # as a pager and bat is not a drop-in for that.
    alias cat='bat --paging=never --style=plain'
    export BAT_THEME="ansi"
    export MANPAGER="sh -c 'col -bx | bat -l man -p'"
fi

# No aliases for `find` or `ps`: fd and procs take different arguments, and
# muscle memory that silently means something else is a trap, not a feature.
command -v dust >/dev/null 2>&1 && alias dud='dust'
command -v duf  >/dev/null 2>&1 && alias dff='duf'
command -v procs >/dev/null 2>&1 && alias psx='procs'
command -v btop >/dev/null 2>&1 && alias top='btop'
command -v lazygit >/dev/null 2>&1 && alias lg='lazygit'
command -v nvim >/dev/null 2>&1 && { alias vi='nvim'; alias vim='nvim'; export EDITOR=nvim; }
[ -n "$EDITOR" ] || export EDITOR=nano
export VISUAL="$EDITOR"

# ── KDOS shorthands ──────────────────────────────────────────────────
alias kfetch='kdos-fetch-app'
alias kstatic='kdos-fetch-static'
alias khelp='kdos help'

# ── fzf ──────────────────────────────────────────────────────────────
if command -v fzf >/dev/null 2>&1; then
    # Phosphor, and no rounded borders.
    export FZF_DEFAULT_OPTS="--height=40% --layout=reverse --border=sharp --info=inline
        --color=bg+:#04120a,bg:#000a03,spinner:#ffb000,hl:#ffb000
        --color=fg:#b8ffc8,header:#12401f,info:#25d0c0,pointer:#39ff14
        --color=marker:#39ff14,fg+:#e8ffee,prompt:#39ff14,hl+:#ffb000
        --color=border:#12401f"
    command -v fd >/dev/null 2>&1 && export FZF_DEFAULT_COMMAND='fd --type f --hidden --follow --exclude .git'
    [ -r /usr/share/fzf/key-bindings.bash ] && . /usr/share/fzf/key-bindings.bash
    [ -r /usr/share/fzf/completion.bash ] && . /usr/share/fzf/completion.bash
fi

# ── zoxide (smarter cd) ──────────────────────────────────────────────
if command -v zoxide >/dev/null 2>&1; then
    eval "$(zoxide init bash --cmd j)"
fi

# ── bash-completion ──────────────────────────────────────────────────
if ! shopt -oq posix; then
    if [ -r /usr/share/bash-completion/bash_completion ]; then
        . /usr/share/bash-completion/bash_completion
    elif [ -r /etc/bash_completion ]; then
        . /etc/bash_completion
    fi
fi

# ── Prompt ───────────────────────────────────────────────────────────
# starship owns the prompt when it is installed; it reads ~/.config/starship.toml,
# whose palette noctalia regenerates with the active colour scheme. The bare
# fallback below is what a distrobox (or a rescue shell) gets. Both mark a
# container so it is always obvious which side of the boundary you are on.
if command -v starship >/dev/null 2>&1; then
    eval "$(starship init bash)"
elif [ -n "${CONTAINER_ID:-}" ]; then
    PS1='\[\033[01;35m\]⬢ '"${CONTAINER_ID}"'\[\033[00m\] \[\033[01;32m\]\u\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\] λ '
else
    PS1='\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\] λ '
fi

# ── Banner ───────────────────────────────────────────────────────────
# Once per terminal, not once per shell: SHLVL is 1 for the shell a tty or foot
# starts and higher for anything nested, so `bash` inside a session stays quiet.
# Skipped inside a container, where the host's specs would be a lie.
if [ "${SHLVL:-1}" -le 1 ] && [ -z "${CONTAINER_ID:-}" ] && [ -t 1 ]; then
    if command -v fastfetch >/dev/null 2>&1; then
        fastfetch
    elif [ -r /etc/motd ]; then
        # `command` because cat is aliased to bat above.
        command cat /etc/motd
    fi
fi
