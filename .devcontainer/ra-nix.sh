#!/usr/bin/env bash
# rust-analyzer started by the VS Code extension does not enter the flake
# dev shell on its own — re-exec it inside `nix develop` so cargo & co. exist.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
exec nix develop --command rust-analyzer "$@"
