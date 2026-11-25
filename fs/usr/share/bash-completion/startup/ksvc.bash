# bash_completion registers a SysV `service <name> <verb>` completion as it
# loads; this startup file runs after it, so ksvc's verbs-then-names hold
# for `service`.
[ -r /usr/share/bash-completion/completions/ksvc ] && . /usr/share/bash-completion/completions/ksvc
