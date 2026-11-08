#!/usr/bin/env python3
# A compositor that offers one thing, org_kde_plasma_window_management, over
# the Wayland wire protocol, for the selftest of kdos-tools' windows.c: the registry,
# the bind, window_with_uuid for every window, and each window's title,
# app id, pid, state and resource name once it is asked for by uuid. A
# window request (set_state, close) is appended to $KDOS_FAKE_LOG.
#
#   fakewm.py <socket path> <windows.tsv> [<ready file>]
#
# windows.tsv: uuid TAB pid TAB state TAB app id TAB resource TAB title
import os, socket, struct, sys

sock_path, table = sys.argv[1], sys.argv[2]
ready = sys.argv[3] if len(sys.argv) > 3 else None
LOG = os.environ.get('KDOS_FAKE_LOG', '/dev/null')
WINDOWS = []
for line in open(table, encoding='utf-8'):
    f = line.rstrip('\n').split('\t')
    if len(f) == 6:
        WINDOWS.append({'uuid': f[0], 'pid': int(f[1]), 'state': int(f[2], 16),
                        'app': f[3], 'res': f[4], 'title': f[5]})

def s_arg(s):
    b = s.encode() + b'\0'
    return struct.pack('<I', len(b)) + b + b'\0' * ((4 - len(b) % 4) % 4)

def msg(obj, op, payload=b''):
    return struct.pack('<II', obj, ((8 + len(payload)) << 16) | op) + payload

def serve(c):
    objs = {1: 'display'}
    buf = b''
    while True:
        d = c.recv(65536)
        if not d:
            return
        buf += d
        out = b''
        while len(buf) >= 8:
            obj, hdr = struct.unpack('<II', buf[:8])
            size, op = hdr >> 16, hdr & 0xffff
            if len(buf) < size:
                break
            body, buf = buf[8:size], buf[size:]
            kind = objs.get(obj)
            if kind == 'display' and op == 1:          # get_registry
                rid = struct.unpack('<I', body[:4])[0]
                objs[rid] = 'registry'
                out += msg(rid, 0, struct.pack('<I', 1) + s_arg('wl_compositor') + struct.pack('<I', 6))
                out += msg(rid, 0, struct.pack('<I', 2) + s_arg('org_kde_plasma_window_management') + struct.pack('<I', 18))
            elif kind == 'display' and op == 0:        # sync
                cid = struct.unpack('<I', body[:4])[0]
                out += msg(cid, 0, struct.pack('<I', 42))
                out += msg(1, 1, struct.pack('<I', cid))  # delete_id
            elif kind == 'registry' and op == 0:       # bind
                name = struct.unpack('<I', body[:4])[0]
                l = struct.unpack('<I', body[4:8])[0]
                p = 8 + l + ((4 - l % 4) % 4)
                ver, nid = struct.unpack('<II', body[p:p + 8])
                if name == 2:
                    objs[nid] = 'pwm'
                    for w in WINDOWS:
                        out += msg(nid, 4, struct.pack('<I', 0) + s_arg(w['uuid']))
            elif kind == 'pwm' and op == 2:            # get_window_by_uuid
                nid = struct.unpack('<I', body[:4])[0]
                uuid = body[8:8 + struct.unpack('<I', body[4:8])[0] - 1].decode()
                w = next((x for x in WINDOWS if x['uuid'] == uuid), None)
                objs[nid] = ('win', uuid)
                if w:
                    out += msg(nid, 0, s_arg(w['title']))
                    out += msg(nid, 1, s_arg(w['app']))
                    out += msg(nid, 2, struct.pack('<I', w['state']))
                    out += msg(nid, 10, struct.pack('<I', w['pid']))
                    out += msg(nid, 16, s_arg(w['res']))
                    out += msg(nid, 6)
                else:
                    out += msg(nid, 5)                   # unmapped
            elif isinstance(kind, tuple) and op == 0:  # set_state
                flags, state = struct.unpack('<II', body[:8])
                with open(LOG, 'a') as f:
                    f.write('set_state %s %x %x\n' % (kind[1], flags, state))
            elif isinstance(kind, tuple) and op == 4:  # close
                with open(LOG, 'a') as f:
                    f.write('close %s\n' % kind[1])
        if out:
            c.sendall(out)

try:
    os.unlink(sock_path)
except FileNotFoundError:
    pass
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(sock_path)
srv.listen(8)
if ready:
    open(ready, 'w').close()
while True:
    c, _ = srv.accept()
    try:
        serve(c)
    finally:
        c.close()
