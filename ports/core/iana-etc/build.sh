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


# THE TABLES ARE BUILT HERE FROM IANA's OWN XML, not carried as generated text,
# so the three files on the image are a function of three hashed registry
# snapshots and nothing else. `version` is the newest <updated> date across the
# three registries; refreshing is a new version, a fetch and new hashes.
#
# gawk BY NAME: the rpc transform uses `delete array` and length() of an
# array, and whichever awk `awk` resolves to in the chroot is not guaranteed
# to have both. The header carries the registry date rather than the build
# time, so two builds of one version are byte-identical. $_reg is the
# recipe's helper, the same base the sources were fetched from.

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

# A record marked Unassigned, Reserved or historic is not a service a
# resolver should answer for, and a name with a parenthesis is a description
# rather than a service name.
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

# IANA names program 100000 "pmapprog"; rpcbind, glibc-era tools and
# rpcinfo look up portmapper and sunrpc, and the NFS helpers look up mountd,
# rstatd, rusersd and rwalld. Those aliases are added where the registry
# lacks them, or the lookups fail on a name every other system resolves.
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

# An empty table is a build that succeeded and a machine on which
# getservbyname() fails for everything; a registry whose layout changed
# produces exactly that, so it stops the build instead.
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
