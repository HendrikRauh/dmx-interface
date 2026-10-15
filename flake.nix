# cspell:words: ESPTOOL_BEFORE
{
  description = "dmx-interface ESP32 Rust development environment (esp-hal, no_std)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    git-hooks.inputs.nixpkgs.follows = "nixpkgs";
    git-hooks.url = "github:cachix/git-hooks.nix";

    esp-rs-nix.url = "github:leighleighleigh/esp-rs-nix";

    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    nixpkgs,
    flake-utils,
    git-hooks,
    esp-rs-nix,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};

        esp-rs = esp-rs-nix.packages.${system}.esp-rs;

        germanDict = pkgs.stdenv.mkDerivation {
          name = "cspell-dict-de";
          src = pkgs.fetchurl {
            url = "https://registry.npmjs.org/@cspell/dict-de-de/-/dict-de-de-4.1.2.tgz";
            hash = "sha256-bikoewusguLv1UvP2x3k9/KpSfBfNP1H88Xfqa1zaUE=";
          };

          # cspell:ignore-words dont
          dontBuild = true;
          dontConfigure = true;
          installPhase = ''
            mkdir -p $out
            cp -r * $out/
          '';
        };

        # Hermetic node_modules for the dev shell, built by nixpkgs'
        # importNpmLock from each package-lock.json. Every package is
        # fetched as its own fixed-output derivation from the lock's
        # integrity field, so there is no hash to maintain: a lock change
        # (renovate, npm update) simply rebuilds the store path.
        webNodeModules = pkgs.importNpmLock.buildNodeModules {
          npmRoot = ./web;
          inherit (pkgs) nodejs;
        };
        siteNodeModules = pkgs.importNpmLock.buildNodeModules {
          npmRoot = ./site;
          inherit (pkgs) nodejs;
        };

        pre-commit-check = git-hooks.lib.${system}.run {
          src = ./.;

          excludes = [
            "\\.bin$"
            "\\.elf$"
            "\\.hex$"
            "\\.o$"
            "^build/"
            "^web/dist/"
            "^web/node_modules/"
            "^site/dist/"
            "^site/node_modules/"
            "^site/.astro/"
            "^flake\\.lock$"
            "^\\.envrc$"
          ];

          hooks = {
            # General
            action-validator.enable = true;
            actionlint.enable = true;
            check-added-large-files = {
              enable = true;
              args = ["--maxkb=1000"];
            };
            check-case-conflicts.enable = true;
            check-symlinks.enable = true;
            end-of-file-fixer.enable = true;
            fix-byte-order-marker.enable = true;
            mixed-line-endings = {
              enable = true;
              args = ["--fix=lf"];
            };
            trim-trailing-whitespace.enable = true;

            # YAML, JSON & TOML
            check-json.enable = true;
            check-toml.enable = true;
            check-yaml.enable = true;
            yamllint = {
              enable = true;
              args = [
                "--strict"
                "-d"
                "{extends: default, rules: {line-length: {max: 120}, document-start: disable}}"
              ];
            };

            # Secrets
            detect-private-keys.enable = true;
            ripsecrets.enable = true;

            # Shell
            check-executables-have-shebangs.enable = true;
            check-shebang-scripts-are-executable = {
              enable = true;
              excludes = ["\\.rs$"];
            };
            shellcheck = {
              enable = true;
              excludes = ["^\\.envrc$"];
            };
            shfmt.enable = true;

            # Python
            python-debug-statements.enable = true;
            ruff-format.enable = true;
            ruff.enable = true;

            # Rust
            rustfmt.enable = true;
            # Sorting only: cargo-sort's formatter indents TOML arrays differently
            # than taplo, so both hooks would rewrite Cargo.toml back and forth.
            cargo-sort = {
              enable = true;
              entry = "${pkgs.cargo-sort}/bin/cargo-sort --no-format";
            };
            rust-docs = {
              enable = true;
              name = "rust-docs";
              description = "Check Rust doc coverage (LEVEL/FILE_DOC in check-rust-docs.sh)";
              entry = "bash ${./.}/scripts/check-rust-docs.sh";
              files = "\\.rs$";
              excludes = ["^build\\.rs$"];
              types = ["file"];
              language = "unsupported";
              pass_filenames = true;
            };
            # Crate-wide rustdoc build; fails on any rustdoc warning (broken
            # intra-doc links etc.). RUSTDOCFLAGS lives in tasks.py
            # (`docs:rustdoc`); the full site build is `invoke docs`.
            rustdoc = {
              enable = true;
              name = "rustdoc";
              description = "Rustdoc must build warning-free (via invoke docs:rustdoc)";
              entry = "invoke docs:rustdoc";
              files = "\\.rs$";
              excludes = ["^build\\.rs$"];
              types = ["file"];
              language = "system";
              pass_filenames = false;
            };
            # Clippy lints the whole crate; it subsumes `cargo check`.
            # Flags live in tasks.py (`_CLIPPY_ARGS`) so there is one source
            # of truth. Must not use --all-targets: `cargo test` cannot build
            # on the no_std xtensa target (no `test` crate).
            cargo-clippy = {
              enable = true;
              name = "cargo-clippy";
              description = "Strict clippy for the xtensa target (via invoke check)";
              entry = "invoke check";
              files = "\\.rs$";
              excludes = ["^build\\.rs$"];
              types = ["file"];
              language = "system";
              pass_filenames = false;
            };

            # TOML
            taplo = {
              enable = true;
              excludes = ["^\\.direnv/"];
            };

            # TypeScript / JSX
            oxfmt = {
              enable = true;
              types_or = [
                "javascript"
                "jsx"
                "ts"
                "tsx"
                "css"
                "scss"
              ];
            };
            oxlint = {
              enable = true;
            };

            # Nix
            alejandra.enable = true;
            deadnix.enable = true;
            flake-checker.enable = true;
            statix.enable = true;

            # Documentation / Spelling
            markdownlint.enable = true;
            # Frontmatter plugin is mandatory: without it mdformat treats YAML
            # frontmatter as Markdown and destroys the docs/ page headers.
            # gfm-alerts keeps `> [!NOTE]` GitHub alerts intact. The upstream
            # hook has no additional_dependencies option — wrap mdformat in a
            # python env that carries the plugins instead.
            mdformat = {
              enable = true;
              args = ["--number"];
              package = pkgs.python3.withPackages (ps: [
                ps.mdformat
                ps.mdformat-frontmatter
                ps.mdformat-gfm-alerts
              ]);
            };
            cspell = {
              enable = true;
              args = ["--no-must-find-files"];
            };
            # AGENTS.md is a budgeted router: line cap + every docs/ pointer
            # must resolve. See the contract header in AGENTS.md.
            agents-md = {
              enable = true;
              name = "agents-md";
              description = "AGENTS.md line budget + docs/ pointer check";
              entry = "bash ${./scripts/check-agents-md.sh}";
              files = "^AGENTS\\.md$|^docs/.*\\.md$";
              language = "system";
              pass_filenames = false;
            };
            # Relative docs/ links: target must exist, #fragments must match
            # a GitHub heading slug. Cheap pre-commit gate for what
            # starlight-links-validator verifies during the astro build.
            md-links = {
              enable = true;
              name = "md-links";
              description = "docs/ relative link + heading-fragment check";
              entry = "python3 ${./scripts/check-md-links.py}";
              files = "^docs/.*\\.md$";
              language = "system";
              pass_filenames = false;
            };

            # Git
            check-merge-conflicts.enable = true;
            convco.enable = true;
          };
        };
      in {
        checks.pre-commit-check = pre-commit-check;

        devShells.default = pkgs.mkShell {
          buildInputs =
            pre-commit-check.enabledPackages
            ++ [
              esp-rs
              pkgs.rustup
              pkgs.rust-analyzer
              pkgs.espflash
              pkgs.esptool
              pkgs.nodejs
              pkgs.taplo
              pkgs.git
              pkgs.libclang
              pkgs.python3.pkgs.invoke
              pkgs.python3.pkgs.pyserial
              pkgs.python3
              pkgs.svgo
              pkgs.renovate
            ];

          env = {
            RUSTUP_TOOLCHAIN = "${esp-rs}";
            LIBCLANG_PATH = "${pkgs.libclang.lib}/lib";
            GERMAN_DICT_PATH = "${germanDict}";
          };

          shellHook =
            pre-commit-check.shellHook
            + ''
              export PATH="$PWD/web/node_modules/.bin:$PWD/site/node_modules/.bin:$PATH"

              (
                set -euo pipefail

                SYNC_LOCK=""

                # Release only a lock we still own: after a steal the dir
                # path is the same but holds the new owner's pid file.
                release_sync_lock() {
                  if [ -n "$SYNC_LOCK" ] && [ "$(cat "$SYNC_LOCK/pid" 2>/dev/null || true)" = "$$" ]; then
                    rm -rf "$SYNC_LOCK"
                  fi
                  SYNC_LOCK=""
                }
                trap 'release_sync_lock' EXIT

                # web: link the store's node_modules into place instead of
                # copying it — idempotent, cheap, and safe when direnv and an
                # explicit `nix develop -c` run the hook at the same time.
                # Local entries such as node_modules/.vite survive because
                # only store-known names are (re)linked.
                link_modules() {
                  local src="$1" dst="node_modules" entry name
                  mkdir -p "$dst"
                  for entry in "$dst"/* "$dst"/.[!.]* "$dst"/..?*; do
                    [ -e "$entry" ] || [ -L "$entry" ] || continue
                    name=$(basename "$entry")
                    if [ -e "$src/$name" ] || [ -L "$src/$name" ]; then
                      continue
                    fi
                    if [ -L "$entry" ]; then
                      case "$(readlink "$entry")" in
                        /nix/store/*) rm -f "$entry" ;;
                      esac
                    else
                      case "$name" in
                        .*) ;;
                        *) rm -rf "$entry" ;;
                      esac
                    fi
                  done
                  for entry in "$src"/* "$src"/.[!.]*; do
                    [ -e "$entry" ] || continue
                    name=$(basename "$entry")
                    if [ -L "$dst/$name" ] && [ "$(readlink "$dst/$name")" = "$entry" ]; then
                      continue
                    fi
                    chmod -R u+w "$dst/$name" 2>/dev/null || true
                    rm -rf "$dst/$name"
                    ln -s "$entry" "$dst/$name"
                  done
                }

                # site: build a real copy. Astro/Starlight's compiler cache
                # rejects symlinked packages ("No cached compile metadata
                # found"), so this one must be an ordinary tree. The lock
                # guards the copy against a concurrent shellHook, and the
                # marker is written while the lock is still held. The lock
                # dir records its owner's PID: a live owner is never
                # interrupted (a cold store copy can take minutes) — only a
                # dead or pre-PID-format owner's lock is stolen.
                copy_modules() {
                  local src="$1" tries=0 owner
                  # XDG_RUNTIME_DIR (not TMPDIR!) is the only location shared
                  # by concurrent entries: nix develop gives every invocation
                  # its own /tmp/nix-shell.*, direnv inherits the unset
                  # caller TMPDIR. Per-user via /run/user/<uid>, with an
                  # explicit uid in the /tmp fallback.
                  SYNC_LOCK="''${XDG_RUNTIME_DIR:-/tmp}/dmx-interface-$(id -u)-$d.nm-sync.lock"
                  while ! mkdir "$SYNC_LOCK" 2>/dev/null; do
                    if [ -f "$MARKER" ] && [ "$(cat "$MARKER")" = "$STAMP" ]; then
                      SYNC_LOCK=""
                      return 0
                    fi
                    tries=$((tries + 1))
                    owner=$(cat "$SYNC_LOCK/pid" 2>/dev/null || true)
                    if [ -z "$owner" ]; then
                      # No PID recorded: leftover from an older shellHook
                      # format (a live holder writes it right after mkdir).
                      if [ "$tries" -ge 30 ]; then
                        echo "WARNING: stealing orphaned sync lock $SYNC_LOCK" >&2
                        rm -rf "$SYNC_LOCK"
                        continue
                      fi
                    elif ! kill -0 "$owner" 2>/dev/null; then
                      echo "WARNING: stealing sync lock $SYNC_LOCK (owner $owner exited)" >&2
                      rm -rf "$SYNC_LOCK"
                      continue
                    elif [ "$tries" -eq 150 ]; then
                      echo "Waiting for sync lock held by PID $owner (concurrent shellHook copying $d/node_modules)..." >&2
                    fi
                    sleep 0.2
                  done
                  printf '%s' "$$" > "$SYNC_LOCK/pid"
                  if [ -f "$MARKER" ] && [ "$(cat "$MARKER")" = "$STAMP" ]; then
                    release_sync_lock
                    return 0
                  fi
                  echo "Copying $d/node_modules from the Nix store (no npm, no hash)..."
                  # A previously interrupted copy leaves store permissions
                  # (dr-xr-xr-x), which `rm -rf` cannot delete — unlock first.
                  chmod -R u+w node_modules 2>/dev/null || true
                  rm -rf node_modules
                  cp -a "$src" node_modules
                  chmod -R u+w node_modules
                  printf '%s' "$STAMP" > "$MARKER"
                  release_sync_lock
                }

                for d in web site; do
                  [ -d "$d" ] || continue
                  cd "$d"

                  case "$d" in
                    web) MODE=link; MODULES=${webNodeModules} ;;
                    site) MODE=copy; MODULES=${siteNodeModules} ;;
                  esac

                  MARKER="node_modules/.nix-store-path"
                  STAMP="$MODE: $MODULES"

                  if [ ! -f "$MARKER" ] || [ "$(cat "$MARKER")" != "$STAMP" ]; then
                    rm -f node_modules/.nix-lockfile.hash
                    case "$MODE" in
                      link)
                        link_modules "$MODULES/node_modules"
                        printf '%s' "$STAMP" > "$MARKER"
                        ;;
                      copy) copy_modules "$MODULES/node_modules" ;;
                    esac
                  fi

                  cd ..
                done
              )
            '';
        };
      }
    );
}
