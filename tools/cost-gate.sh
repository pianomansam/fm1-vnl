#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Task 7c's measured cost gate (timing is noisy, so it is not in the unit suite): runs the --parts bench and checks
# that the rows the half-rate keys and pads and the dust's fast path target each dropped by at least half the
# shortlist's lower estimate (docs/cost.md): Rhodes, felt piano and Wurli by 0.10, pad and vox pad by 0.05, the idle
# floor by 0.025.
#   tools/cost-gate.sh BEFORE.txt [AFTER.txt]   BEFORE/AFTER: saved `cost-bench.sh --parts` output (AFTER: run now)
set -e
cd "$(dirname "$0")/.."
[ -f "$1" ] || { echo "usage: tools/cost-gate.sh BEFORE.txt [AFTER.txt]"; exit 2; }
AFTER=${2:-app/build/bench/parts-now.txt}
[ -n "$2" ] || sh tools/cost-bench.sh --parts > "$AFTER"
awk '
function ratio(line) { sub(/.*ratio /, "", line); return line + 0 }
FNR == 1 { f++ }
/^floor: idle/ { v[f, "idle floor"] = ratio($0) }
/^chords/ { n = $0; sub(/^chords +/, "", n); sub(/ +[0-9.]+ ms.*/, "", n); v[f, n] = ratio($0) }
END {
    split("rhodes:0.10,felt piano:0.10,wurli:0.10,pad:0.05,vox pad:0.05,idle floor:0.025", g, ",")
    bad = 0
    for (i = 1; i in g; i++) {
        split(g[i], kv, ":")
        d = v[1, kv[1]] - v[2, kv[1]]
        ok = d >= kv[2] - 1e-9
        printf "%-11s %.3f -> %.3f  saved %.3f (gate %.3f) %s\n", kv[1], v[1, kv[1]], v[2, kv[1]], d, kv[2], ok ? "ok" : "FAIL"
        if (!ok) bad = 1
    }
    print bad ? "cost gate: FAIL" : "cost gate: ok"
    exit bad
}' "$1" "$AFTER"
