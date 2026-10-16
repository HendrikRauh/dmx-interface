#!/usr/bin/env bash
# AGENTS.md is a budgeted router (see its contract header):
#   1. line budget — every line must earn its place;
#   2. pointer integrity — every docs/*.md referenced must exist.
set -euo pipefail

MAX_LINES=160
AGENTS="AGENTS.md"
FAIL=0

if [ ! -f "$AGENTS" ]; then
	echo "agents-md: $AGENTS not found" >&2
	exit 1
fi

LINES=$(wc -l <"$AGENTS")
if [ "$LINES" -gt "$MAX_LINES" ]; then
	echo "agents-md: $AGENTS is $LINES lines (budget $MAX_LINES)." >&2
	echo "Delete something first — see the contract header in $AGENTS." >&2
	FAIL=1
fi

# Every docs/<page>.md mentioned in AGENTS.md must exist.
while read -r ref; do
	if [ ! -f "$ref" ]; then
		echo "agents-md: $AGENTS references missing file: $ref" >&2
		FAIL=1
	fi
done < <(grep -oE 'docs/[a-z0-9-]+\.md' "$AGENTS" | sort -u)

exit "$FAIL"
