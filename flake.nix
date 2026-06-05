# cspell:words: ESPTOOL_BEFORE
{
  description = "dmx-interface development environment";

  inputs = {
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    git-hooks.inputs.nixpkgs.follows = "nixpkgs";
    git-hooks.url = "github:cachix/git-hooks.nix";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    esp-dev,
    git-hooks,
    nixpkgs,
    ...
  }: let
    esp-idf = esp-dev.packages.${system}.esp-idf-full;
    pkgs = nixpkgs.legacyPackages.${system};
    system = "x86_64-linux";

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
        "^assets/case/"
        "^build/"
        "^dependencies\\.lock$"
        "^docs/doxygen/"
        "^docs/external/"
        "^flake\\.lock$"
        "^latex/"
        "^managed_components/"
        "^sdkconfig$"
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
        editorconfig-checker = {
          enable = true;
          excludes = [
            "\\.c$"
            "\\.cpp$"
            "\\.h$"
            "\\.hpp$"
            "\\.md$"
            "^\\.envrc$"
          ];
        };
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
        check-shebang-scripts-are-executable.enable = true;
        shellcheck = {
          enable = true;
          excludes = ["^\\.envrc$"];
        };
        shfmt.enable = true;

        # Python
        python-debug-statements.enable = true;
        ruff-format.enable = true;
        ruff.enable = true;

        # Documentation
        doxygen-coverage = {
          enable = true;
          name = "doxygen code coverage";
          entry = "tools/doxy-coverage.py docs/doxygen/xml --threshold=100 --generate-docs";
          files = "\\.(c|cc|cxx|cxxm|cpp|cppm|ccm|c++|c++m|java|ii|ixx|ipp|i++|inl|idl|ddl|odl|h|hh|hxx|hpp|h++|l|cs|d|php|php4|php5|phtml|inc|m|markdown|md|mm|dox|py|pyw|f90|f95|f03|f08|f18|f|for|vhd|vhdl|ucf|qsf|ice)$"; # cspell:disable-line
          pass_filenames = false;
        };
        markdownlint.enable = true;
        mdformat = {
          enable = true;
          args = ["--number"];
        };
        cspell = {
          enable = true;
          args = ["--no-must-find-files"];
        };

        # C/C++ & Build-Systems
        clang-format = {
          enable = true;
          types_or = [
            "c"
            "c++"
          ];
          args = ["-i"];
        };
        cmake-format.enable = true;

        # Web-Files
        prettier = {
          enable = true;
          types_or = [
            "json"
            "css"
            "scss"
            "html"
            "yaml"
          ];
          excludes = ["\\.md$"];
          args = [
            "--write"
            "--ignore-unknown"
          ];
        };
        html-tidy = {
          enable = true;
          files = "\\.(html|htm)$";
          excludes = ["^assets/doxygen/.*$"];
        };
        oxfmt = {
          enable = true;
          args = ["--config" "web/.oxfmtrc.json"];
        };
        oxlint = {
          enable = true;
          args = ["--config" "web/.oxlintrc.json"];
        };

        # Nix
        alejandra.enable = true;
        deadnix.enable = true;
        flake-checker.enable = true;
        statix.enable = true;

        # Git
        check-merge-conflicts.enable = true;
        convco.enable = true;
      };
    };
  in {
    checks.${system}.pre-commit-check = pre-commit-check;

    devShells.${system}.default = pkgs.mkShell {
      buildInputs =
        pre-commit-check.enabledPackages
        ++ [
          esp-idf
          pkgs.clang-tools
          pkgs.doxygen
          pkgs.graphviz
          pkgs.python3
          pkgs.python3Packages.invoke
          pkgs.svgo
          pkgs.nodejs
        ];
      shellHook =
        pre-commit-check.shellHook
        + ''
          export ESPTOOL_BEFORE=usb_reset
          export PATH="$PWD/web/node_modules/.bin:$PATH"

          # Set up cspell dictionary files
          mkdir -p .cspell
          ln -sfn ${germanDict} .cspell/dict-de-de

          # Install packages from package.json in a sub shell if there are changes in package-lock.json
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

                # Update the stored hash so we don't install again next time
                echo "$CURRENT_HASH" > "$HASH_STORE"
              fi
            else
              echo "Warning: No $LOCKFILE found. Run 'npm install' manually to create one."
            fi
          )
        '';
    };
  };
}
