"""Record what a real program writes to a terminal, once, into a fixture.

HOW THE .esc FILES BESIDE THIS ONE WERE MADE, and it is here so that knowledge
is recoverable rather than because anything re-runs it: a re-recording picks up
a different program version, a different terminfo and a different hostname, so a
fixture that regenerated itself would be a test that changed its own question.

Run it from a container that has vim, htop, mc, less and tmux, with the
repository as the working directory. `vim -n` matters — without it a killed vim
leaves a swap file in the tree it was reading.
"""
import fcntl, os, pty, select, signal, struct, termios, time

OUT = os.path.dirname(os.path.abspath(__file__))

def record(name, argv, script, settle=0.6, cwd="."):
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(cwd)
        os.environ["TERM"] = "xterm-256color"
        os.environ["LANG"] = "C.UTF-8"
        os.environ["COLUMNS"] = "80"
        os.environ["LINES"] = "24"
        for v in ("HOME",):
            os.environ[v] = "/tmp"
        try:
            os.execvp(argv[0], argv)
        except Exception:
            pass
        os._exit(127)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    buf = bytearray()

    def drain(t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if not r:
                continue
            try:
                d = os.read(fd, 65536)
            except OSError:
                return False
            if not d:
                return False
            buf.extend(d)
        return True

    if not drain(settle):
        raise SystemExit("%s: the program did not run" % name)
    for keys, wait in script:
        try:
            os.write(fd, keys)
        except OSError:
            break
        if not drain(wait):
            break
    try:
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)
    os.close(fd)
    with open(os.path.join(OUT, name), "wb") as f:
        f.write(bytes(buf))
    print("  %-14s %6d bytes" % (name, len(buf)))

#
# EACH ENDS ON A LIVE FRAME, never on the program's exit. A stream that ended
# with the alternate screen being restored would render to an empty grid, and an
# empty grid is what a parser that crashed on the first byte also produces — the
# golden could not tell them apart. The alternate screen's own restore is
# asserted directly in the libkvt block instead, where it is two escapes rather
# than two kilobytes.
#
record("vim.esc", ["vim", "-n", "-u", "NONE", "-U", "NONE", "-i", "NONE", "README.md"],
       [(b"G", 0.5)], settle=1.2)
record("htop.esc", ["htop", "-d", "20"], [], settle=1.8)
record("mc.esc", ["mc", "-c", "-d"], [], settle=1.8)
record("less.esc", ["less", "-X", "README.md"], [(b" ", 0.5)], settle=0.8)
record("tmux.esc", ["tmux", "-f", "/dev/null", "new-session"],
       [(b'\x02"', 0.8)], settle=1.2)

# Deliberately malformed: a CSI that never ends, a parameter list past any
# sane bound, an OSC with no terminator, a truncated UTF-8 lead byte, and a
# DCS carrying rubbish. Written rather than recorded — no program emits these,
# and that is the point.
bad = (b"ok\r\n"
       b"\x1b[" + b"9;" * 40 + b"m"
       b"\x1b[999999999999999999999H"
       b"\x1b]0;title-with-no-terminator"
       b"\x1b\\"
       b"\xc3"
       b"\xed\xa0\x80"
       b"\x1bPqrubbish"
       b"\x1b[?\x1b[m"
       b"\x1b[38;2;300;300;300m"
       b"still here\r\n")
with open(os.path.join(OUT, "malformed.esc"), "wb") as f:
    f.write(bad)
print("  %-14s %6d bytes" % ("malformed.esc", len(bad)))
