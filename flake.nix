{
  description = "dmx-interface development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    git-hooks.url = "github:cachix/git-hooks.nix";
    git-hooks.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = {
    nixpkgs,
    esp-dev,
    git-hooks,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
    esp-idf = esp-dev.packages.${system}.esp-idf-full;

    pre-commit-check = git-hooks.lib.${system}.run {
      src = ./.;

      excludes = [
        "^build/"
        "^managed_components/"
        "\\.bin$"
        "\\.elf$"
        "\\.hex$"
        "\\.o$"
        "^flake\\.lock$"
        "^dependencies\\.lock$"
        "^assets/case/"
        "^docs/doxygen/"
        "^docs/external/"
        "^latex/"
        "^sdkconfig$"
      ];

      hooks = {
        # Allgemeine Datei-Checks
        end-of-file-fixer.enable = true;
        fix-byte-order-marker.enable = true;
        mixed-line-endings = {
          enable = true;
          args = ["--fix=lf"];
        };
        trim-trailing-whitespace.enable = true;
        check-added-large-files = {
          enable = true;
          args = ["--maxkb=1000"];
        };
        check-case-conflicts.enable = true;
        check-json.enable = true;
        check-merge-conflicts.enable = true;
        detect-private-keys.enable = true;
        check-symlinks.enable = true;
        action-validator.enable = true;
        actionlint.enable = true;
        ripsecrets.enable = true;
        typos.enable = true;
        editorconfig-checker = {
          enable = true;
          excludes = ["\\.c$" "\\.h$" "\\.cpp$" "\\.hpp$" "\\.md$" "^\\.envrc$"];
        };
        check-yaml.enable = true;
        check-toml.enable = true;

        # Shell Skript Validierung
        shellcheck = {
          enable = true;
          excludes = ["^\\.envrc$"];
        };
        shfmt.enable = true;
        check-executables-have-shebangs.enable = true;
        check-shebang-scripts-are-executable.enable = true;

        # Python & Linters
        ruff.enable = true;
        ruff-format.enable = true;
        python-debug-statements.enable = true;
        mdformat.enable = true;

        yamllint = {
          enable = true;
          args = [
            "--strict"
            "-d"
            "{extends: default, rules: {line-length: {max: 120}, document-start: disable}}"
          ];
        };
        markdownlint.enable = true;

        # C/C++ & Build-Systeme
        cmake-format.enable = true;
        clang-format = {
          enable = true;
          types_or = [
            "c"
            "c++"
          ];
          args = ["-i"];
        };

        # Web-Files (HTML, CSS, JS) & Formatierung
        html-tidy.enable = true; # Validiert HTML Struktur
        prettier = {
          enable = true;
          types_or = [
            "javascript"
            "jsx"
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

        # Nix Quality Assurance (Mit Alejandra)
        alejandra.enable = true; # Der kompromisslose Formatierer
        deadnix.enable = true;
        statix.enable = true;
        flake-checker.enable = true;

        # Typst Support
        typstyle.enable = true;

        # Git
        convco.enable = true;

        # Custom Hook (Doxygen)
        doxygen-coverage = {
          enable = true;
          name = "doxygen code coverage";
          entry = "tools/doxy-coverage.py docs/doxygen/xml --threshold=100 --generate-docs";
          files = "\\.(c|h|cpp|hpp)$";
          pass_filenames = false;
        };
      };
    };
  in {
    checks.${system}.pre-commit-check = pre-commit-check;

    devShells.${system}.default = pkgs.mkShell {
      inherit (pre-commit-check) shellHook;
      buildInputs =
        pre-commit-check.enabledPackages
        ++ [
          esp-idf
          pkgs.python3
          pkgs.python3Packages.invoke
          pkgs.clang-tools
          pkgs.svgo
          pkgs.doxygen
          pkgs.graphviz
        ];
    };
  };
}
