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
            "^flake\\.lock$"
            "^\\.envrc$"
          ];

          hooks = {
            # General
            check-added-large-files = {
              enable = true;
              args = ["--maxkb=1000"];
            };
            check-case-conflicts.enable = true;
            check-merge-conflicts.enable = true;
            check-toml.enable = true;
            check-yaml.enable = true;
            check-json.enable = true;
            end-of-file-fixer.enable = true;
            fix-byte-order-marker.enable = true;
            mixed-line-endings = {
              enable = true;
              args = ["--fix=lf"];
            };
            trim-trailing-whitespace.enable = true;

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
            # intra-doc links etc.). RUSTDOCFLAGS lives in tasks.py (`docs`).
            rustdoc = {
              enable = true;
              name = "rustdoc";
              description = "Rustdoc must build warning-free (via invoke docs)";
              entry = "invoke docs";
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
              excludes = ["\\.svg"];
            };
            oxlint = {
              enable = true;
              excludes = ["\\.svg"];
            };

            # Nix
            alejandra.enable = true;
            deadnix.enable = true;
            flake-checker.enable = true;
            statix.enable = true;

            # Documentation / Spelling
            markdownlint.enable = true;
            mdformat = {
              enable = true;
              args = ["--number"];
            };
            cspell = {
              enable = true;
              args = ["--no-must-find-files"];
            };

            # SCSS / CSS
            prettier = {
              enable = true;
              types_or = [
                "scss"
                "css"
              ];
            };
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
              export PATH="$PWD/web/node_modules/.bin:$PATH"

              (
                set -euo pipefail
                cd web

                LOCKFILE="package-lock.json"
                HASH_STORE="node_modules/.nix-lockfile.hash"

                if [ -f "$LOCKFILE" ]; then
                  CURRENT_HASH=$(sha256sum "$LOCKFILE" | cut -d' ' -f1)
                  if [ ! -d "node_modules" ] || [ ! -f "$HASH_STORE" ] || [ "$(cat "$HASH_STORE")" != "$CURRENT_HASH" ]; then
                    echo "Changes detected in $LOCKFILE. Running npm install..."
                    npm install

                    echo "$CURRENT_HASH" > "$HASH_STORE"
                  fi
                else
                  echo "Warning: No $LOCKFILE found. Run 'npm install' manually to create one."
                fi
              )
            '';
        };
      }
    );
}
