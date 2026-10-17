#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

gawk -F '[<>]' -v URL="$_reg/protocol-numbers/protocol-numbers.xml" -v V="$version" '
	BEGIN {
		print "# /etc/protocols, IANA registry as of " V
		print "# Source: " URL
	}
	/<record/ {v = n = ""}
	/<value/ {v = $3}
	/<name/ && $3 !~ / / {n = $3}
	/<\/record/ && n && v != "" {
		printf "%-12s %3i %s\n", tolower(n), v, n
	}
' < protocol-numbers-$version.xml > protocols

gawk -F '[<>]' -v URL="$_reg/service-names-port-numbers/service-names-port-numbers.xml" -v V="$version" '
	BEGIN {
		print "# /etc/services, IANA registry as of " V
		print "# Source: " URL
	}
	/<record/ {n = u = p = c = ""}
	/<name/ && !/\(/ {n = $3}
	/<number/ {u = $3}
	/<protocol/ {p = $3}
	/Unassigned/ || /Reserved/ || /historic/ {c = 1}
	/<\/record/ && n && u && p && !c {
		printf "%-15s %5i/%s\n", n, u, p
	}
' < service-names-port-numbers-$version.xml > services

gawk -F '[<>]' -v URL="$_reg/rpc-program-numbers/rpc-program-numbers.xml" -v V="$version" '
	BEGIN {
		print "# /etc/rpc, IANA registry as of " V
		print "# Source: " URL
	}
	/<record/ {v = n = c = ""}
	/<value/ {v = $3}
	/<name/ {n = $3}
	/Unassigned/ || /Reserved/ || /\[unknown\]/ {c = 1}
	/<\/record/ && n && v != "" && !c {
		sub(/#.*/, "", n)
		gsub(/[[:space:]]+/, " ", n)
		sub(/^ /, "", n)
		sub(/ $/, "", n)
		if (n == "") next
		if (v == "100000" && n !~ /portmapper/) n = n " portmapper sunrpc"
		if (v == "100001" && n !~ /rstatd/) n = "rstatd " n
		if (v == "100002" && n !~ /rusersd/) n = "rusersd " n
		if (v == "100005" && n !~ /mountd/) n = "mountd " n " mount"
		if (v == "100008" && n !~ /rwalld/) n = "rwalld " n " rwall"
		split(n, names, " ")
		delete seen
		printf "%-15s %s", names[1], v
		seen[names[1]] = 1
		for (i = 2; i <= length(names); i++) {
			if (names[i] in seen) continue
			printf " %s", names[i]
			seen[names[i]] = 1
		}
		printf "\n"
	}
' < rpc-program-numbers-$version.xml > rpc

for _f in services protocols rpc; do
	if ! grep -qv '^#' "$_f"; then
		echo "ERROR: iana-etc: $_f came out empty" >&2
		exit 1
	fi
done
grep -q '^ssh  *22/tcp$' services || { echo "ERROR: iana-etc: services has no ssh" >&2; exit 1; }
grep -q '^tcp  *6 TCP$' protocols || { echo "ERROR: iana-etc: protocols has no tcp" >&2; exit 1; }

install -d -m 755 $PKG/etc
install -m 644 services protocols rpc $PKG/etc
