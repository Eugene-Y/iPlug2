#!/usr/bin/env python3
"""Resolve git conflict blocks in a file. Usage: resolve.py <file> <strategy>[,<strategy>...]
strategy per conflict block (in order; last one repeats): ours | theirs | both (ours then theirs) | theirs-then-ours"""
import sys, re
path, strategies = sys.argv[1], sys.argv[2].split(',')
lines = open(path).read().split('\n')
out, i, n = [], 0, 0
while i < len(lines):
    if lines[i].startswith('<<<<<<<'):
        ours, theirs, j = [], [], i + 1
        while not lines[j].startswith('|||||||') and not lines[j].startswith('======='):
            ours.append(lines[j]); j += 1
        if lines[j].startswith('|||||||'):
            while not lines[j].startswith('======='): j += 1
        j += 1
        while not lines[j].startswith('>>>>>>>'):
            theirs.append(lines[j]); j += 1
        s = strategies[min(n, len(strategies) - 1)]; n += 1
        out += {'ours': ours, 'theirs': theirs, 'both': ours + theirs, 'theirs-then-ours': theirs + ours}[s]
        i = j + 1
    else:
        out.append(lines[i]); i += 1
open(path, 'w').write('\n'.join(out))
print(f"{path}: resolved {n} block(s)")
