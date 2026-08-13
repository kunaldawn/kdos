#!/usr/bin/env python3
"""Own a qemu unix socket (serial or monitor): log everything to <name>.log,
accept lines to send on <name>.fifo.  One owner, so nothing interleaves."""
import os, socket, sys, threading, time

sock_path, name = sys.argv[1], sys.argv[2]
log_path, fifo_path = name + ".log", name + ".fifo"

for _ in range(100):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(sock_path)
        break
    except OSError:
        time.sleep(0.2)
else:
    sys.exit("cannot connect " + sock_path)

if not os.path.exists(fifo_path):
    os.mkfifo(fifo_path)
log = open(log_path, "ab", buffering=0)


def reader():
    while True:
        try:
            d = s.recv(65536)
        except OSError:
            break
        if not d:
            break
        log.write(d)


threading.Thread(target=reader, daemon=True).start()

while True:
    with open(fifo_path, "rb") as f:      # blocks until a writer opens
        for line in f:
            if line.strip() == b"__QUIT__":
                sys.exit(0)
            if line.startswith(b"__RAW__ "):
                s.sendall(line[8:].rstrip(b"\n").decode("unicode_escape").encode("latin1"))
            else:
                s.sendall(line)
