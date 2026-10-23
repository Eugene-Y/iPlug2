#!/usr/bin/env bash
# Print the call-trace.sh symbol lines for iPlug's VST3 latency and state path in a built plug-in: each method and
# its non-virtual thunks (_ZThn<offset>_…), whose offsets change with the class layout, so a .syms file written for
# one iPlug2 revision misses the hosts' calls in another.
# Usage: make-vst3-syms.sh <plugin .so> [extra method regex]
SO="$1"; EXTRA="${2:-}"
M='10SetLatencyEi|17getLatencySamplesEv|9setActiveEh|13setProcessingEh|8getStateEPN9Steinberg8IBStreamE|8setStateEPN9Steinberg8IBStreamE'
[ -n "$EXTRA" ] && M="$M|$EXTRA"
nm "$SO" | awk '{print $NF}' | grep -E "^(_ZN|_ZThn[0-9]+_N)5iplug9IPlugVST3($M)\$" | sort -u
