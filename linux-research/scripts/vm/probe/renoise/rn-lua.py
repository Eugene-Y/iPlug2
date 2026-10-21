#!/usr/bin/env python3
"""Run Lua in a running Renoise through its OSC server (/renoise/evaluate; Preferences > OSC, UDP 8000).
Usage: rn-lua.py '<lua chunk>'   The chunk may return a value: it is shown in Renoise's status bar (bottom left),
since OSC has no reply and the evaluate sandbox has neither io nor a print that reaches stdout. Read it from a
screenshot (vm-shot.sh)."""
import socket, sys


def osc_string(s):
    b = s.encode() + b"\0"
    return b + b"\0" * (-len(b) % 4)


lua = ("local ok, r = pcall(function() %s end) "
       "renoise.app():show_status('LUA ' .. (ok and tostring(r) or ('ERROR ' .. tostring(r))))" % sys.argv[1])
msg = osc_string("/renoise/evaluate") + osc_string(",s") + osc_string(lua)
socket.socket(socket.AF_INET, socket.SOCK_DGRAM).sendto(msg, ("127.0.0.1", 8000))
