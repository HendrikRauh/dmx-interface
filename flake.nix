{
  description = "dmx-interface development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs =
    {
      self,
      nixpkgs,
      esp-dev,
    }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      esp-idf = esp-dev.packages.${system}.esp-idf-full;
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [
          esp-idf
          pkgs.python3
          pkgs.python3Packages.invoke

          pkgs.pre-commit
          pkgs.clang-tools
          pkgs.svgo
          pkgs.prettier

          pkgs.nixfmt
          pkgs.doxygen
          pkgs.graphviz
        ];
      };
    };
}
