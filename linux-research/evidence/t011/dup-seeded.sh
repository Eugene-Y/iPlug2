#!/bin/bash
# dup-seeded.sh <Examples-dir> <args to duplicate.py...>: run duplicate.py with a fixed RNG seed (stable PLUG_UNIQUE_ID).
set -e
ex="$1"; shift
cd "$ex"
python3 -c 'import random, runpy, sys; random.seed(11); sys.argv = ["duplicate.py"] + sys.argv[1:]; runpy.run_path("duplicate.py", run_name="__main__")' "$@"
