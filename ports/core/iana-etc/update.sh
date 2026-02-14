#!/bin/sh -
#@ Update protocols and services from IANA.
#@ Taken from ArchLinux script written by Gaetan Bisson.  Adjusted for CRUX.

awk=awk
curl=curl
url_pn='https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xml'
url_snpn="https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xml"
url_rpc="https://www.iana.org/assignments/rpc-program-numbers/rpc-program-numbers.xml"

download() {
	datetime=`date +'%FT%T%z'`
	echo 'Downloading protocols'
	${curl} -o protocols.xml ${url_pn}
	[ ${?} -eq 0 ] || exit 20
	echo 'Downloading services'
	${curl} -o services.xml ${url_snpn}
	[ ${?} -eq 0 ] || exit 21
	echo 'Downloading rpc'
	${curl} -o rpc.xml ${url_rpc}
	[ ${?} -eq 0 ] || exit 22
}

process() {
	echo 'Processing protocols'
	${awk} -F "[<>]" -v URL="${url_pn}" -v DT="${datetime}" '
		BEGIN{
			print "# /etc/protocols, created " DT
			print "# Source: " URL
		}
		/<record/ {v = n = ""}
		/<value/ {v = $3}
		/<name/ && $3!~/ / {n = $3}
		/<\/record/ && n && v != ""{
			printf "%-12s %3i %s\n", tolower(n), v, n
		}
	' < protocols.xml > protocols.new
	[ ${?} -eq 0 ] || exit 30

	echo 'Processing services'
	${awk} -F "[<>]" -v URL="${url_snpn}" -v DT="${datetime}" '
		BEGIN{
			print "# /etc/services, created " DT
			print "# Source: " URL
		}
		/<record/ {n = u = p = c = ""}
		/<name/ && !/\(/ {n = $3}
		/<number/ {u = $3}
		/<protocol/ {p = $3}
		/Unassigned/ || /Reserved/ || /historic/ {c = 1}
		/<\/record/ && n && u && p && !c{
			printf "%-15s %5i/%s\n", n, u, p
		}
	' < services.xml > services.new
	[ ${?} -eq 0 ] || exit 31

	echo 'Processing rpc'
	${awk} -F "[<>]" -v URL="${url_rpc}" -v DT="${datetime}" '
		BEGIN{
			print "# /etc/rpc, created " DT
			print "# Source: " URL
		}
		/<record/ {v = n = c = ""}
		/<value/ {v = $3}
		/<name/ {n = $3}
		/Unassigned/ || /Reserved/ || /\[unknown\]/ {c = 1}
		/<\/record/ && n && v != "" && !c{
			sub(/#.*/, "", n)
			gsub(/[[:space:]]+/, " ", n)
			sub(/^ /, "", n)
			sub(/ $/, "", n)
			if (n == "") next
			# Add common aliases for compatibility if they are missing
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
	' < rpc.xml > rpc.new
	[ ${?} -eq 0 ] || exit 32
}

update() {
	mv protocols.new protocols
	[ ${?} -eq 0 ] || exit 40
	mv services.new services
	[ ${?} -eq 0 ] || exit 41
	mv rpc.new rpc
	[ ${?} -eq 0 ] || exit 44
	rm -f protocols.xml services.xml rpc.xml
	[ ${?} -eq 0 ] || exit 42
}

download
process
update
