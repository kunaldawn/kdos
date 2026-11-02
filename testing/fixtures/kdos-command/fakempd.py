#!/usr/bin/env python3
# mpd's protocol, faked for the selftest of kdos-mpctl's MPRIS bridge: the
# greeting, `status`, `currentsong`, the transport and mixer verbs, and `idle`,
# which answers `changed: player` on every connection parked in it as soon as
# another connection changes the state. Every command is appended to
# $KDOS_FAKE_LOG.
#
#   fakempd.py <socket path> [<ready file>]
import os, socket, sys, threading

path = sys.argv[1]
ready = sys.argv[2] if len(sys.argv) > 2 else None
LOG = os.environ.get('KDOS_FAKE_LOG', '/dev/null')
state = {'state': 'stop', 'volume': 40, 'random': 0, 'repeat': 0}
lock = threading.Condition()
gen = [0]

def log(s):
    with open(LOG, 'a') as f:
        f.write(s + '\n')

def changed():
    with lock:
        gen[0] += 1
        lock.notify_all()

def serve(c):
    f = c.makefile('rwb', buffering=0)
    f.write(b'OK MPD 0.24.0\n')
    seen = gen[0]
    for raw in f:
        cmd = raw.decode().strip()
        if not cmd:
            continue
        log('mpd ' + cmd)
        words = cmd.split()
        out = ''
        if words[0] == 'status':
            out = ('volume: %d\nrepeat: %d\nrandom: %d\nstate: %s\nsongid: 7\n'
                   'elapsed: 12.500\nduration: 200.000\n' % (state['volume'],
                   state['repeat'], state['random'], state['state']))
        elif words[0] == 'currentsong':
            out = 'file: music/Song.flac\nTitle: A Song\nArtist: Someone\nAlbum: Record\nId: 7\n'
        elif words[0] == 'idle':
            with lock:
                while gen[0] == seen:
                    lock.wait()
                seen = gen[0]
            out = 'changed: player\n'
        elif words[0] in ('play',):
            state['state'] = 'play'; changed()
        elif words[0] == 'pause':
            state['state'] = 'pause' if (words[1:] or ['1'])[0] == '1' else 'play'; changed()
        elif words[0] == 'stop':
            state['state'] = 'stop'; changed()
        elif words[0] == 'setvol':
            state['volume'] = int(words[1]); changed()
        elif words[0] in ('random', 'repeat'):
            state[words[0]] = int(words[1]); changed()
        elif words[0] in ('next', 'previous', 'seekcur'):
            changed()
        else:
            f.write(b'ACK [5@0] {%s} unknown command\n' % words[0].encode())
            continue
        f.write(out.encode() + b'OK\n')

os.makedirs(os.path.dirname(path), exist_ok=True)
try:
    os.unlink(path)
except FileNotFoundError:
    pass
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(path)
srv.listen(8)
if ready:
    open(ready, 'w').close()
while True:
    c, _ = srv.accept()
    threading.Thread(target=serve, args=(c,), daemon=True).start()
