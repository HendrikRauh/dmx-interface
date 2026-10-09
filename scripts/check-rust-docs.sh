#!/usr/bin/env bash
# Check Rust documentation coverage.
# Usage: check-rust-docs.sh [files...]
# Without files, scans src/ recursively.
set -euo pipefail

# ── Toggle ──
# Level: public = pub items only | private = all items
LEVEL="private"
# File docs: true = check //! at the top of every .rs file
FILE_DOC=true
# ────────────

errors=0

check_item_doc() {
	local file="$1" lineno="$2" line="$3"

	if echo "$line" | grep -qE 'pub\s+mod\s+'; then return; fi

	# Out-of-line module (`mod x;`): documented via `//!` at the top of the
	# module file — checked by check_file_doc, not by the `///` lines above
	# the declaration (which must stay bare, see AGENTS.md).
	if echo "$line" | grep -qE '^\s*(pub(\(.+\))?\s+)?mod\s+\w+\s*;'; then return; fi

	if [ "$lineno" -le 1 ]; then
		echo "MISSING DOCS: $file:$lineno:$line"
		errors=$((errors + 1))
		return
	fi

	local found=0 check=$((lineno - 1))
	for _ in 1 2 3 4 5; do
		if [ "$check" -le 0 ]; then break; fi
		local prev_line
		prev_line=$(sed -n "${check}p" "$file")
		if printf '%s\n' "$prev_line" | grep -qE '^\s*///'; then
			found=1
			break
		fi
		if ! printf '%s\n' "$prev_line" | grep -qE '^\s*(#\[|//!|$)'; then break; fi
		check=$((check - 1))
	done

	if [ "$found" -eq 0 ]; then
		echo "MISSING DOCS: $file:$lineno:$line"
		errors=$((errors + 1))
	fi
}

check_file_doc() {
	local file="$1"
	if ! head -5 "$file" | grep -qE '^\s*//!'; then
		echo "MISSING FILE DOC: $file"
		errors=$((errors + 1))
	fi
}

process_file() {
	local file="$1"

	if [ "$FILE_DOC" = true ]; then
		check_file_doc "$file"
	fi

	local pattern='^\s*(pub(\(.+\))?\s+)?(async\s+)?(fn|struct|enum|trait|type|const|static|mod)\s+\w+'
	[ "$LEVEL" = "public" ] && pattern='^\s*pub(\(.+\))?\s+(async\s+)?(fn|struct|enum|trait|type|const|static|mod)\s+\w+'

	while IFS= read -r match; do
		check_item_doc "$file" "${match%%:*}" "${match#*:}"
	done < <(rg -n "$pattern" "$file" 2>/dev/null || true)
}

if [ $# -gt 0 ]; then
	for file in "$@"; do
		[[ $file == *.rs ]] || continue
		[ -f "$file" ] || continue
		process_file "$file"
	done
else
	while IFS= read -r file; do process_file "$file"; done < <(find src -name '*.rs' -type f)
fi

if [ "$errors" -gt 0 ]; then
	echo ""
	echo "FAILED: $errors item(s) missing documentation."
	exit 1
fi

echo "OK: all items documented."
