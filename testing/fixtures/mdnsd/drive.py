# drive.py <kdos-mdnsd> <scratch> — the parser, the answer builder and the
# daemon's event loop, against crafted packets and a fake avahi-daemon.
#
# THE PACKETS are built here, not recorded, so each case names the one thing
# that is wrong with it: a truncated question, a pointer to itself, a pointer
# chain that loops, a label running past the end. A parser that follows a
# loop never returns, so every --answer run has a timeout and a hang fails.
#
# THE FAKE AVAHI is a unix socket speaking the simple protocol: a name
# starting `slow` is never answered, `v4only` answers IPv4 and holds IPv6,
# `drop` is hung up on before a reply line, and every other name answers
# both. The daemon runs as whoever runs the suite on a port the kernel
# picks; as root it drops to `nobody`, which cannot reach a socket under the
# suite's 0700 scratch directory, so then the socket goes in a directory of
# its own that `nobody` may enter.
import os, shutil, socket, struct, subprocess, sys, tempfile, threading, time
binary, tmp = sys.argv[1], sys.argv[2]

def name(n):
    return b"".join(bytes([len(l)]) + l.encode() for l in n.split(".")) + b"\0"

def query(n, qtype=1, qid=0x1234, flags=0x0100, qd=1, raw=None):
    q = raw if raw is not None else name(n) + struct.pack(">HH", qtype, 1)
    return struct.pack(">HHHHHH", qid, flags, qd, 0, 0, 0) + q

def decode(p):
    qid, fl, qd, an, ns, ar = struct.unpack(">HHHHHH", p[:12])
    o = 12
    def skip(o):
        while True:
            c = p[o]
            if c & 0xc0 == 0xc0: return o + 2
            if c == 0: return o + 1
            o += 1 + c
    for _ in range(qd): o = skip(o) + 4
    recs = []
    for _ in range(an + ns):
        o = skip(o)
        t, c, ttl, rl = struct.unpack(">HHIH", p[o:o + 10]); o += 10
        rd = p[o:o + rl]; o += rl
        if t == 1: rd = socket.inet_ntop(socket.AF_INET, rd)
        elif t == 28: rd = socket.inet_ntop(socket.AF_INET6, rd)
        recs.append((t, ttl, rd))
    if o != len(p): raise SystemExit("  trailing bytes in the answer")
    return qid, fl & 0xf, an, ns, recs

def answer(pkt, reply):
    f = os.path.join(tmp, "q.bin")
    open(f, "wb").write(pkt)
    try:
        r = subprocess.run([binary, "--answer", f, reply], capture_output=True,
                           timeout=10)
    except subprocess.TimeoutExpired:
        print("  the parser did not return on %r" % pkt); sys.exit(1)
    return r.returncode, r.stdout

def need(cond, what):
    if not cond:
        print("  " + what); sys.exit(1)

rc, out = answer(query("kdos-test.local"), "+ 2 0 kdos-test.local 10.0.2.15")
qid, rcode, an, ns, recs = decode(out)
need(qid == 0x1234 and rcode == 0 and recs == [(1, 10, "10.0.2.15")],
     "a good A question did not get Avahi's address at TTL 10: %r" % (recs,))
rc, out = answer(query("Printer.Local", 28), "+ 2 1 Printer.local fec0::27b3")
need(decode(out)[4] == [(28, 10, "fec0::27b3")], "a good AAAA question went unanswered")
rc, out = answer(query("printer.local", 28), "+ 2 1 printer.local fe80::1")
_, rcode, an, ns, recs = decode(out)
need(rcode == 0 and an == 0 and ns == 1 and recs[0][0] == 6,
     "a link-local AAAA was answered instead of an empty answer with an SOA")
rc, out = answer(query("nosuch.local"), "-15 Timeout reached")
_, rcode, an, ns, recs = decode(out)
need(rcode == 0 and an == 0 and ns == 1 and recs[0][1] == 10,
     "a miss was not NOERROR, no answer and an SOA at TTL 10 (rcode %d)" % rcode)
rc, out = answer(query("kdos-test.local"), "")
need(decode(out)[1:3] == (0, 0), "no reply from Avahi was not an empty answer")
rc, out = answer(query("kdos-test.local", 16), "")
need(decode(out)[1:4] == (0, 0, 1), "a TXT question under .local was not an empty answer")
rc, out = answer(query("example.com"), "+ 2 0 example.com 10.0.0.1")
need(decode(out)[1] == 5 and decode(out)[2] == 0, "a name outside .local was not REFUSED")
rc, out = answer(query("local"), "+ 2 0 local 10.0.0.1")
need(decode(out)[1] == 5, "the bare `local` was not REFUSED")
rc, out = answer(query("a b.local"), "+ 2 0 a 10.0.0.1")
need(decode(out)[1] == 5, "a name with a space reached Avahi's command line")
rc, out = answer(query("x.local")[:20], "")
need(decode(out)[1] == 1, "a truncated question was not FORMERR")
rc, out = answer(query("x.local")[:9], "")
need(rc == 3 and out == b"", "a packet shorter than a header was answered")
rc, out = answer(query("x.local", flags=0x8100), "")
need(rc == 3, "a response packet was answered")
loop = query("", raw=b"\x01a\xc0\x0e\x01b\xc0\x0c" + struct.pack(">HH", 1, 1))
rc, out = answer(loop, "+ 2 0 a.local 10.0.0.1")
need(decode(out)[1] == 1, "a compression loop was not FORMERR")
self_ptr = query("", raw=b"\xc0\x0c" + struct.pack(">HH", 1, 1))
need(decode(answer(self_ptr, "")[1])[1] == 1, "a pointer to itself was not FORMERR")
past = query("", raw=b"\x3fabc")
need(decode(answer(past, "")[1])[1] == 1, "a label past the packet was not FORMERR")
need(decode(answer(query("x.local", qd=2), "")[1])[1] == 1, "two questions were not FORMERR")
print("  A, AAAA, misses, REFUSED, truncation and compression loops: as specified")

# The daemon, against a fake Avahi on a socket of the test's own.
sockdir = tmp
if os.geteuid() == 0:
    sockdir = tempfile.mkdtemp(prefix="kdos-mdnsd.")
    os.chmod(sockdir, 0o755)
sock = os.path.join(sockdir, "avahi.sock")
if os.path.exists(sock): os.unlink(sock)
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(sock); srv.listen(64)
os.chmod(sock, 0o777)
held = []
def serve():
    while True:
        try: c, _ = srv.accept()
        except OSError: return
        line = c.makefile("rb").readline().decode().split()
        cmd, host = line[0], line[1]
        if host.startswith("slow") or (host.startswith("v4only") and cmd.endswith("6")):
            held.append(c); continue
        if host.startswith("drop"):
            c.close(); continue
        addr = "10.0.2.15" if cmd.endswith("4") else "fec0::5"
        c.sendall(("+ 2 %d %s %s\n" % (0 if cmd.endswith("4") else 1, host, addr)).encode())
        c.close()
threading.Thread(target=serve, daemon=True).start()

d = subprocess.Popen([binary, "--port", "0", "--socket", sock],
                     stderr=subprocess.PIPE)
try:
    first = d.stderr.readline().decode()
    need("listening on 127.0.0.1:" in first, "the daemon did not start: %r" % first)
    port = int(first.split("127.0.0.1:")[1].split(",")[0])
    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    u.settimeout(5)
    t0 = time.monotonic()
    u.sendto(query("slow.local", qid=1), ("127.0.0.1", port))
    u.sendto(query("fast.local", qid=2), ("127.0.0.1", port))
    got = decode(u.recv(512))
    fast = time.monotonic() - t0
    need(got[0] == 2 and got[4] == [(1, 10, "10.0.2.15")] and fast < 1,
         "fast.local waited behind slow.local (%.1f s, %r)" % (fast, got))
    got = decode(u.recv(512))
    slow = time.monotonic() - t0
    need(got[0] == 1 and got[2] == 0 and 2.5 < slow < 4,
         "slow.local was not an empty answer at three seconds (%.1f s)" % slow)
    t0 = time.monotonic()
    u.sendto(query("v4only.local", 28, qid=3), ("127.0.0.1", port))
    time.sleep(0.1)
    u.sendto(query("v4only.local", 1, qid=4), ("127.0.0.1", port))
    a = decode(u.recv(512)); b = decode(u.recv(512))
    took = time.monotonic() - t0
    need(a[0] == 4 and b[0] == 3 and b[2] == 0 and took < 1.5,
         "an IPv4-only host's AAAA waited %.1f s after its A answered" % took)
    u.sendto(query("example.com", qid=5), ("127.0.0.1", port))
    need(decode(u.recv(512))[1] == 5, "the daemon did not refuse a name outside .local")
    u.sendto(b"\x00\x01garbage", ("127.0.0.1", port))
    u.sendto(query("fast.local", 28, qid=6), ("127.0.0.1", port))
    got = decode(u.recv(512))
    need(got[0] == 6 and got[4] == [(28, 10, "fec0::5")],
         "the daemon stopped answering after a broken packet")
    u.sendto(query("drop.local", qid=7), ("127.0.0.1", port))
    got = decode(u.recv(512))
    need(got[0] == 7 and got[1] == 2 and got[4] == [],
         "a connection Avahi ended before a reply was not SERVFAIL: %r" % (got,))
    need(d.poll() is None, "the daemon exited")
    print("  the daemon answers one name while another is pending, times out at 3 s,")
    print("  and says SERVFAIL when Avahi hangs up")
finally:
    d.kill(); d.wait(); srv.close()
    for c in held: c.close()
    if sockdir != tmp: shutil.rmtree(sockdir, ignore_errors=True)
