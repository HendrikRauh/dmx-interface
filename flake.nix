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
          files = "\\.(c|h|cpp|hpp)$";
          pass_filenames = false;
        };
        markdownlint.enable = true;
        mdformat = {
          enable = true;
          args = ["--number"];
        };
        cspell.enable = true;

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
      inherit (pre-commit-check) shellHook;
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
        ];
    };
  };
}
