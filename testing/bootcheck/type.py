#!/usr/bin/env python3
"""Type a string into the guest's VT via qemu monitor sendkey."""
import os, sys, time

RUN = os.environ.get("KDOS_BOOTCHECK_RUN", "/tmp/kdos-bootcheck")
FIFO = os.path.join(RUN, "mon.fifo")

MAP = {
    ' ': 'spc', '-': 'minus', '=': 'equal', '.': 'dot', '/': 'slash',
    ',': 'comma', ';': 'semicolon', "'": 'apostrophe', '\\': 'backslash',
    '[': 'bracket_left', ']': 'bracket_right', '`': 'grave_accent',
    '\n': 'ret', '\t': 'tab',
}
SHIFTED = {
    '_': 'minus', '+': 'equal', ':': 'semicolon', '"': 'apostrophe',
    '?': 'slash', '<': 'comma', '>': 'dot', '|': 'backslash', '~': 'grave_accent',
    '!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6', '&': '7',
    '*': '8', '(': '9', ')': '0', '{': 'bracket_left', '}': 'bracket_right',
}


def keys(s):
    out = []
    for ch in s:
        if ch in MAP:
            out.append(MAP[ch])
        elif ch in SHIFTED:
            out.append('shift-' + SHIFTED[ch])
        elif ch.isdigit():
            out.append(ch)
        elif ch.isalpha():
            out.append(('shift-' if ch.isupper() else '') + ch.lower())
        else:
            raise SystemExit('unmapped char %r' % ch)
    return out


text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
text = text.replace('\\n', '\n')
with open(FIFO, 'wb') as f:
    for k in keys(text):
        f.write(('sendkey %s\n' % k).encode())
        f.flush()
        time.sleep(0.03)
