#!/usr/bin/env python3
"""Run a shell command on the guest serial console; print just its output."""
import os, re, sys, time

RUN = os.environ.get("KDOS_BOOTCHECK_RUN", "/tmp/kdos-bootcheck")
LOG, FIFO = os.path.join(RUN, "serial.log"), os.path.join(RUN, "serial.fifo")
timeout = float(os.environ.get("KQ_TIMEOUT", "60"))
cmd = " ".join(sys.argv[1:])
t = str(int(time.time() * 1000) % 10000000)
BEG, END = "@B" + t + "@", "@E" + t + "@"

start = os.path.getsize(LOG)
with open(FIFO, "wb") as f:
    f.write(("echo %s; { %s ; } 2>&1; echo %s$?\n" % (BEG, cmd, END)).encode())

deadline, txt, rc = time.time() + timeout, "", None
while time.time() < deadline:
    with open(LOG, "rb") as f:
        f.seek(start)
        raw = f.read()
    txt = raw.decode("utf-8", "replace")
    txt = re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]", "", txt).replace("\r", "")
    txt = re.sub(r"\x1b\][^\x07]*\x07", "", txt)
    i = txt.rfind(BEG + "\n")
    if i >= 0:
        body = txt[i + len(BEG) + 1:]
        m = re.search(re.escape(END) + r"(\d+)", body)
        if m:
            rc = m.group(1)
            print(body[:m.start()].rstrip())
            break
    time.sleep(0.25)
else:
    sys.stderr.write("[[TIMEOUT %ss]] tail:\n%s\n" % (timeout, txt[-1500:]))
    sys.exit(99)
if rc != "0":
    sys.stderr.write("[[exit %s]]\n" % rc)
