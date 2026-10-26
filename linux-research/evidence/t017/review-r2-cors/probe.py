import os, socket, struct, subprocess, sys, time
sys.argv = [sys.argv[0], sys.argv[1]]
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../scripts/t017/helper-probe.py')).read()
head = src.split("ok = pump(10, 'Ready')")[0]
exec(head)
pump(10, 'Ready')
a.sendall(frame('LoadURL', 'http://127.0.0.1:8765/remote.html'))
pump(8)
a.close(); time.sleep(1)
