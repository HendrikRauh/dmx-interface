# cspell:words: ESPTOOL_BEFORE pyproject cadquery Patchelf virtualenv opencascade occt dont vtkmodules
{
  description = "dmx-interface development environment";

  inputs = {
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    git-hooks.inputs.nixpkgs.follows = "nixpkgs";
    git-hooks.url = "github:cachix/git-hooks.nix";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    pyproject-nix.url = "github:pyproject-nix/pyproject.nix";
    uv2nix = {
      url = "github:pyproject-nix/uv2nix";
      inputs.pyproject-nix.follows = "pyproject-nix";
    };
    pyproject-build-systems = {
      url = "github:pyproject-nix/build-system-pkgs";
      inputs = {
        pyproject-nix.follows = "pyproject-nix";
        uv2nix.follows = "uv2nix";
        nixpkgs.follows = "nixpkgs";
      };
    };
  };

  outputs = {
    esp-dev,
    git-hooks,
    nixpkgs,
    pyproject-build-systems,
    pyproject-nix,
    uv2nix,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
    esp-idf = esp-dev.packages.${system}.esp-idf-full;
    inherit (pkgs) lib;

    workspace = uv2nix.lib.workspace.loadWorkspace {workspaceRoot = ./.;};

    python = pkgs.python3;

    pythonBase = pkgs.callPackage pyproject-nix.build.packages {inherit python;};

    overlay = workspace.mkPyprojectOverlay {
      sourcePreference = "wheel";
    };

    pythonSet = pythonBase.overrideScope (
      lib.composeManyExtensions [
        pyproject-build-systems.overlays.wheel
        overlay

        (_: prev: {
          cadquery-ocp = prev.cadquery-ocp.overrideAttrs (_: {
            dontAutoPatchelf = true;
          });
        })
      ]
    );

    virtualenv = pythonSet.mkVirtualEnv "dmx-env" workspace.deps.default;

    germanDict = pkgs.stdenv.mkDerivation {
      name = "cspell-dict-de";
      src = pkgs.fetchurl {
        url = "https://registry.npmjs.org/@cspell/dict-de-de/-/dict-de-de-4.1.2.tgz";
        hash = "sha256-bikoewusguLv1UvP2x3k9/KpSfBfNP1H88Xfqa1zaUE=";
      };

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
        "^assets/case/output/"
        "^assets/case/parts/"
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
            "CMakeLists\\.txt$"
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
      shellHook = ''
        git lfs install --local --force
        ${pre-commit-check.shellHook}
      '';

      buildInputs =
        pre-commit-check.enabledPackages
        ++ [
          esp-idf
          pkgs.clang-tools
          pkgs.doxygen
          pkgs.graphviz
          pkgs.opencascade-occt
          pkgs.python3
          pkgs.python3Packages.invoke
          pkgs.svgo
          pkgs.uv
          pkgs.vtk
          virtualenv
        ];
      env = {
        ESPTOOL_BEFORE = "usb_reset";
        GERMAN_DICT_PATH = "${germanDict}";
        LD_LIBRARY_PATH = "${pkgs.lib.makeLibraryPath [
          pkgs.opencascade-occt
          pkgs.vtk
          pkgs.stdenv.cc.cc.lib
          pkgs.libGL
          pkgs.libX11
          pkgs.expat
          pkgs.zlib
        ]}:${virtualenv}/lib/python3.13/site-packages/vtkmodules:${virtualenv}/lib/python3.13/site-packages/cadquery_vtk:$LD_LIBRARY_PATH";
        UV_NO_SYNC = "1";
        UV_PYTHON = python.interpreter;
        UV_PYTHON_DOWNLOADS = "never";
      };
    };
  };
}
