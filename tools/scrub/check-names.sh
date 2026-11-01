#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The lead-style name scrub's check (Sam: lead styles go by genre name only; no composer or performer behind any
# lead style stays in the repo). The name list lives outside the repo, because the list itself is the names.
#   tools/scrub/check-names.sh NAMES tree             tracked files' contents and paths
#   tools/scrub/check-names.sh NAMES history RANGE    every commit in RANGE: its message, its diff and its paths
# NAMES: one fixed string per line, matched case-insensitively as a whole word; '#' lines and blank lines skipped.
# Prints each hit and exits 1 if there are any, 0 if none.
# Out of scope (Sam): older research, which names lo-fi artists for other reasons; its files are skipped in both
# modes (their paths are below).
set -u

usage() { echo "usage: $0 NAMES tree | $0 NAMES history RANGE" >&2; exit 2; }
[ $# -ge 2 ] || usage
names=$1
mode=$2
range=${3-}
[ -r "$names" ] || { echo "$0: can't read $names" >&2; exit 2; }
case $mode in tree) ;; history) [ -n "$range" ] || usage ;; *) usage ;; esac

cd "$(git rev-parse --show-toplevel)" || exit 2
tmp=$(mktemp -d "${TMPDIR:-/tmp}/check-names.XXXXXX") || exit 2
trap 'rm -rf "$tmp"' EXIT
grep -v -e '^#' -e '^[[:space:]]*$' "$names" > "$tmp/pat"

set -- ':(exclude)docs/research/fm1-accompaniment-code.md' \
       ':(exclude)docs/research/lofi-hip-hop.md' \
       ':(exclude)docs/research/lofi-midi-arrangements.md' \
       ':(exclude)docs/research/other-firmwares.md' \
       ':(exclude)docs/music/ideas-from-other-apps.md'

if [ "$mode" = tree ]; then
    git grep -n -I -i -w -F -f "$tmp/pat" -- . "$@" > "$tmp/hits"
    git ls-files -- . "$@" | grep -i -w -F -f "$tmp/pat" | sed 's/^/path: /' >> "$tmp/hits"
else
    # messages: every commit in the range; diffs and paths: the in-scope files only
    git log --format='@@commit %h%n%B' "$range" -- > "$tmp/msg" || exit 2
    git log -p --format='@@commit %h' "$range" -- . "$@" > "$tmp/diff" || exit 2
    # each line's commit and file go to a side file, so that only the line itself is matched
    awk -v tag="$tmp/tag" '/^@@commit /{c=$2; next} {print c " msg:" > tag; print}' "$tmp/msg" > "$tmp/body"
    awk -v tag="$tmp/tag" '/^@@commit /{c=$2; next} /^diff --git /{f=$3; sub(/^a\//, "", f)}
        {print c " " f ":" >> tag; print}' "$tmp/diff" >> "$tmp/body"
    grep -n -i -w -F -f "$tmp/pat" "$tmp/body" \
        | awk -v tag="$tmp/tag" 'BEGIN{while ((getline t < tag) > 0) T[++n] = t}
            {i = index($0, ":"); print T[substr($0, 1, i - 1)] " " substr($0, i + 1)}' > "$tmp/hits"
fi

if [ -s "$tmp/hits" ]; then
    cat "$tmp/hits"
    n=$(wc -l < "$tmp/hits" | tr -d ' ')
    if [ "$mode" = history ]; then
        c=$(cut -d' ' -f1 "$tmp/hits" | sort -u | wc -l | tr -d ' ')
        echo "check-names: $n hit lines in $c commits" >&2
    else
        echo "check-names: $n hit lines" >&2
    fi
    exit 1
fi
echo "check-names: no hits ($mode)" >&2
exit 0
