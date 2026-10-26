#!/usr/bin/env python3
"""Drives iplug2-webview-helper directly over its socket, without a plug-in: load, script messages, evaluate.
Runs INSIDE the GUI container. Usage: helper-probe.py <helper> [parent-window-id]"""
import os, socket, struct, subprocess, sys, time

T = dict(LoadHTML=1, LoadURL=2, Reload=3, Evaluate=4, SetZoom=5, EnableInteraction=6, NavigationReply=7,
         MimeTypeReply=8, DownloadPathReply=9, Ready=100, ContentLoaded=101, ScriptMessage=102,
         EvaluateResult=103, NavigationRequest=104, MimeTypeRequest=105, DownloadPathRequest=106,
         DownloadProgress=107, DownloadFinished=108, FocusRequest=109, WebProcessGone=110)
NAMES = {v: k for k, v in T.items()}
LAYOUT = {'Ready': 'ii', 'ContentLoaded': '', 'ScriptMessage': 's', 'EvaluateResult': 'iis',
          'NavigationRequest': 'is', 'MimeTypeRequest': 'is', 'DownloadPathRequest': 'is',
          'DownloadProgress': 'ii', 'DownloadFinished': 'is', 'FocusRequest': 'i', 'WebProcessGone': 'i'}

def frame(kind, *fields):
    body = struct.pack('=I', T[kind])
    for f in fields:
        if isinstance(f, int):
            body += struct.pack('=q', f)
        else:
            b = f.encode()
            body += struct.pack('=I', len(b)) + b
    return struct.pack('=I', len(body)) + body

def parse(buf):
    out = []
    while len(buf) >= 4:
        (n,) = struct.unpack_from('=I', buf)
        if len(buf) < 4 + n:
            break
        body = buf[4:4 + n]; buf = buf[4 + n:]
        (t,) = struct.unpack_from('=I', body); pos = 4; name = NAMES.get(t, t); vals = []
        for c in LAYOUT.get(name, ''):
            if c == 'i':
                vals.append(struct.unpack_from('=q', body, pos)[0]); pos += 8
            else:
                (l,) = struct.unpack_from('=I', body, pos); pos += 4
                vals.append(body[pos:pos + l].decode(errors='replace')); pos += l
        out.append((name, vals))
    return out, buf

helper = sys.argv[1]
parent = sys.argv[2] if len(sys.argv) > 2 else subprocess.check_output(
    "xwininfo -root | awk '/Window id/{print $4}'", shell=True).decode().strip()
a, b = socket.socketpair()
os.set_inheritable(b.fileno(), True)
pid = os.fork()
if pid == 0:
    os.dup2(b.fileno(), 3)
    os.execv(helper, [helper, '--parent', parent, '--width', '400', '--height', '300', '--devtools', '1',
                      '--scheme', 'iplug2'])
b.close()
a.settimeout(0.2)
buf = b''
log = []
def pump(secs, until=None):
    global buf
    end = time.time() + secs
    while time.time() < end:
        try:
            d = a.recv(65536)
            if not d:
                log.append(('EOF', [])); return False
            buf += d
        except socket.timeout:
            pass
        msgs, buf = parse(buf)
        for m in msgs:
            log.append(m); print('<-', m, flush=True)
            if m[0] in ('NavigationRequest', 'MimeTypeRequest'):
                a.sendall(frame('NavigationReply' if m[0] == 'NavigationRequest' else 'MimeTypeReply', m[1][0], 1 if m[0] == 'NavigationRequest' else 0))
            if until and m[0] == until:
                return True
    return False

ok = pump(10, 'Ready')
a.sendall(frame('LoadHTML', "<html><body><input id=t><script>IPlugSendMsg({msg:'hello', n: 3});</script></body></html>"))
pump(10, 'ContentLoaded')
a.sendall(frame('Evaluate', 7, "1 + 2"))
a.sendall(frame('Evaluate', 8, "({a: 'x', b: [1, 2]})"))
a.sendall(frame('Evaluate', 9, "throw new Error('boom')"))
a.sendall(frame('Evaluate', 0, "IPlugSendMsg({msg: 'from-eval'})"))
pump(3)
open('/tmp/t017-probe.html', 'w').write("<html><head><script type=module src='mod.js'></script></head><body>scheme</body></html>")
open('/tmp/mod.js', 'w').write("IPlugSendMsg({msg: 'module-loaded', origin: location.href});")
a.sendall(frame('LoadURL', 'iplug2:/tmp/t017-probe.html'))
pump(5)
a.close()
time.sleep(1.5)
gone = os.waitpid(pid, os.WNOHANG)
print('helper exited after close:', gone, flush=True)
