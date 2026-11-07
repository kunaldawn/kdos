#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   fakeoomd.py SOCKET ANSWER — kdos-oomd's socket, as far as kdos-notice asks
#
# Answers `status` with the line in the file ANSWER, read afresh on every
# connection, so a test changes the kill count by rewriting one file. Any
# other word gets the daemon's own refusal.
# ---------------------------------
import os
import socket
import sys

SOCK, ANSWER = sys.argv[1], sys.argv[2]
try:
    os.unlink(SOCK)
except FileNotFoundError:
    pass
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(SOCK)
s.listen(8)
while True:
    c, _ = s.accept()
    try:
        word = c.recv(64).decode(errors="replace").strip()
        if word == "status":
            with open(ANSWER) as f:
                c.sendall((f.read().strip() + "\n").encode())
        else:
            c.sendall(b"err unknown command\n")
    finally:
        c.close()
