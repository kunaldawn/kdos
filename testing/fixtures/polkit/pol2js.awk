# <table> is set with -v: _shipped or _built. One line per action.
/<action[ \t]/ {
    if (match($0, /id="[^"]*"/)) {
        id = substr($0, RSTART + 4, RLENGTH - 5)
        any = "no"; ina = "no"; act = "no"; open = 1
    }
}
open && /<allow_any>/ {
    v = $0; sub(/.*<allow_any>[ \t]*/, "", v); sub(/[ \t]*<\/allow_any>.*/, "", v); any = v
}
open && /<allow_inactive>/ {
    v = $0; sub(/.*<allow_inactive>[ \t]*/, "", v); sub(/[ \t]*<\/allow_inactive>.*/, "", v); ina = v
}
open && /<allow_active>/ {
    v = $0; sub(/.*<allow_active>[ \t]*/, "", v); sub(/[ \t]*<\/allow_active>.*/, "", v); act = v
}
open && /<\/action>/ {
    printf "polkit.%s[\"%s\"] = [\"%s\", \"%s\", \"%s\"];\n", table, id, any, ina, act
    open = 0
}
