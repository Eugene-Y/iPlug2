#!/usr/bin/env python3
"""N3: copy Sand-vst3's non-format sources (Sand, hvoya, plog, HIIR) into Sand-aax.vcxproj(.filters).
Edits lines in place so the BOM, LF endings, order and every other setting stay byte for byte.
Usage: n3-sync-aax-sources.py <Sand/projects>"""
import re
import sys
from pathlib import Path

proj = Path(sys.argv[1])
FORMAT = re.compile(r'VST3_SDK|\\IPlug\\VST3\\|\\IPlug\\AAX\\|AAX_SDK')


def read(name):
    return (proj / name).read_bytes().decode('utf-8-sig').split('\n')


def write(name, lines):
    (proj / name).write_bytes(b'\xef\xbb\xbf' + '\n'.join(lines).encode('utf-8'))


def item_blocks(lines, tag):
    """Each <tag Include=...> item as (path, [lines]), self-closing or with children."""
    out, i = [], 0
    while i < len(lines):
        m = re.match(r'\s*<%s Include="([^"]+)"(\s*/)?>' % tag, lines[i])
        if not m:
            i += 1
            continue
        j = i
        if not m.group(2):
            while f'</{tag}>' not in lines[j]:
                j += 1
        out.append((m.group(1), lines[i:j + 1]))
        i = j + 1
    return out


def norm(p):
    return p.replace('/', '\\').lower()


def missing(src_lines, dst_lines, tag):
    have = {norm(p) for p, _ in item_blocks(dst_lines, tag)}
    return [(p, b) for p, b in item_blocks(src_lines, tag) if not FORMAT.search(p) and norm(p) not in have]


def insert_after_last(lines, tag, blocks):
    last = max(i for i, l in enumerate(lines)
               if re.match(r'\s*<%s Include="[^"]+"\s*/>' % tag, l) or l.strip() == f'</{tag}>')
    return lines[:last + 1] + [l for _, b in blocks for l in b] + lines[last + 1:]


def drop(lines, tag, path):
    return [l for l in lines if not re.match(r'\s*<%s Include="%s"\s*/>' % (tag, re.escape(path)), l)]


for suffix in ('', '.filters'):
    src, dst = read('Sand-vst3.vcxproj' + suffix), read('Sand-aax.vcxproj' + suffix)
    dst = drop(dst, 'ClInclude', r'..\Sand.h')   # the template's name; Sand's header is Sand.hpp
    for tag in ('ClCompile', 'ClInclude'):
        add = missing(src, dst, tag)
        print(f'Sand-aax.vcxproj{suffix}: + {len(add)} {tag}: ' + ', '.join(p.split('\\')[-1] for p, _ in add))
        dst = insert_after_last(dst, tag, add)
    if suffix and not any('<Filter Include="hvoya">' in l for l in dst):
        start = next(i for i, l in enumerate(src) if '<Filter Include="hvoya">' in l)
        end = next(i for i in range(start, len(src)) if '</Filter>' in src[i])
        last = max(i for i, l in enumerate(dst) if l.strip() == '</Filter>')
        dst = dst[:last + 1] + src[start:end + 1] + dst[last + 1:]
        print('Sand-aax.vcxproj.filters: + filter "hvoya"')
    write('Sand-aax.vcxproj' + suffix, dst)
