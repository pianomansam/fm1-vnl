#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Task 7c's measured cost gate (timing is noisy, so it is not in the unit suite): runs the --parts bench and checks
# that the rows each move targets dropped by at least half the shortlist's lower estimate (docs/cost.md): moves 1-2,
# the Rhodes, felt piano and Wurli rows by 0.10 and the pad by 0.05 (and the vox pad, removed 2026-10-31: no row); move 3 (the dust's fast path and the
# quarter-rate plate), the always-on chain alone by 0.025. The idle floor is printed, not gated: it falls mostly
# because the engine skips work in silence.
#   tools/cost-gate.sh BEFORE.txt [AFTER.txt]   BEFORE/AFTER: saved `cost-bench.sh --parts` output (AFTER: run now)
set -e
cd "$(dirname "$0")/.."
[ -f "$1" ] || { echo "usage: tools/cost-gate.sh BEFORE.txt [AFTER.txt]"; exit 2; }
AFTER=${2:-app/build/bench/parts-now.txt}
mkdir -p app/build/bench
[ -n "$2" ] || sh tools/cost-bench.sh --parts > "$AFTER"
# AMB-1's time drifts by up to 10% between runs on a busy host, so both sides are divided by the BEFORE run's.
awk '
function net(line) { sub(/.*net +/, "", line); return line + 0 }
FNR == 1 { f++ }
/^floor: idle/ { a = $0; sub(/.*amb /, "", a); if (f == 1) amb = a + 0; t = $0; sub(/.*silence: /, "", t); ms[f, "idle floor"] = t + 0 }
/^chords/ { n = $0; sub(/^chords +/, "", n); sub(/ +[0-9.]+ ms.*/, "", n); ms[f, n] = net($0) }
/^fx +chain alone/ { ms[f, "chain alone"] = net($0) }
END { for (k in ms) { split(k, kk, SUBSEP); v[kk[1], kk[2]] = ms[k] / amb } }
END {
    printf "(ratios over the before run'"'"'s AMB-1 time, %.1f ms)\n", amb
    split("rhodes:0.10,felt piano:0.10,wurli:0.10,pad:0.05,chain alone:0.025", g, ",")
    bad = 0
    for (i = 1; i in g; i++) {
        split(g[i], kv, ":")
        d = v[1, kv[1]] - v[2, kv[1]]
        ok = d >= kv[2] - 1e-9
        printf "%-11s %.3f -> %.3f  saved %.3f (gate %.3f) %s\n", kv[1], v[1, kv[1]], v[2, kv[1]], d, kv[2], ok ? "ok" : "BELOW THE ESTIMATE"
        if (!ok) bad = 1
    }
    printf "%-11s %.3f -> %.3f  saved %.3f (not gated: mostly the silence skip)\n", "idle floor", v[1, "idle floor"], v[2, "idle floor"], v[1, "idle floor"] - v[2, "idle floor"]
    print bad ? "cost gate: FAIL" : "cost gate: ok"
    exit bad
}' "$1" "$AFTER"
