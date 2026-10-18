#!/usr/bin/env python3
"""Relative markdown link check for docs/ (pre-commit gate).

Validates every relative link in docs/**.md without building the site: the
target file must exist, and `page.md#fragment` must match a GitHub heading
slug (or an explicit HTML id) in the target file. This is the cheap gate for
the failure mode starlight-links-validator catches during the production
build.

Skipped, matching what the same files render as on GitHub: external schemes,
root-absolute paths (site routes such as /api/…), and fenced code blocks.
"""

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote

SKIP_SCHEMES = ("http:", "https:", "mailto:", "tel:", "data:", "javascript:")
LINK_RE = re.compile(r"!?\[[^\]]*\]\(\s*([^)\s]+?)(?:\s+[\"'][^\"']*[\"'])?\s*\)")
HEADING_RE = re.compile(r"^#{1,6}\s+(.+?)\s*#*\s*$")
ID_RE = re.compile(r"""\bid=["']([^"']+)["']""")
FENCE_RE = re.compile(r"^\s*(?:```|~~~)")


def slugify(text: str) -> str:
    """Approximate github-slugger: lowercase, drop punctuation, spaces → `-`."""
    text = re.sub(r"[^\w\s-]", "", text.lower())
    return re.sub(r"\s", "-", text.strip())


def content_lines(path: Path) -> list[str]:
    """File lines with fenced code blocks blanked out."""
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    in_fence = False
    out = []
    for line in lines:
        if FENCE_RE.match(line):
            in_fence = not in_fence
            out.append("")
        else:
            out.append("" if in_fence else line)
    return out


def heading_slugs(path: Path) -> set[str]:
    """All heading slugs (deduped like GitHub) plus explicit HTML ids."""
    slugs: set[str] = set()
    seen: dict[str, int] = {}
    for line in content_lines(path):
        if m := HEADING_RE.match(line):
            base = slugify(m.group(1))
            n = seen.get(base, 0)
            seen[base] = n + 1
            slugs.add(base if n == 0 else f"{base}-{n}")
        slugs.update(ID_RE.findall(line))
    return slugs


def check_file(src: Path, root: Path, cache: dict[Path, set[str]]) -> list[str]:
    errors: list[str] = []
    for lineno, line in enumerate(content_lines(src), 1):
        for m in LINK_RE.finditer(line):
            target = unquote(m.group(1))
            if target.startswith(SKIP_SCHEMES) or target.startswith("/"):
                continue
            path_part, _, fragment = target.partition("#")
            dest = (src.parent / path_part).resolve() if path_part else src.resolve()
            if not dest.exists():
                errors.append(
                    f"{src.relative_to(root)}:{lineno}: missing target {target}"
                )
                continue
            if fragment and dest.suffix == ".md":
                slugs = cache.setdefault(dest, heading_slugs(dest))
                if fragment not in slugs:
                    errors.append(
                        f"{src.relative_to(root)}:{lineno}: dead fragment {target}"
                    )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "docs",
        nargs="?",
        default="docs",
        help="markdown directory to scan (default: docs)",
    )
    args = parser.parse_args()

    docs = Path(args.docs)
    if not docs.is_dir():
        print(f"✗ {docs} does not exist", file=sys.stderr)
        return 1

    root = docs.parent.resolve()
    files = sorted(p.resolve() for p in docs.rglob("*.md"))
    if not files:
        print(f"✗ no markdown files under {docs}", file=sys.stderr)
        return 1

    errors: list[str] = []
    cache: dict[Path, set[str]] = {}
    for path in files:
        errors.extend(check_file(path, root, cache))

    if errors:
        print(f"✗ {len(errors)} broken link(s) in {docs}:")
        for error in errors:
            print(f"  {error}")
        return 1

    print(f"✓ {len(files)} files, all relative links and fragments resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
