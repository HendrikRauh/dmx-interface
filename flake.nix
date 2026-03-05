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

          # Formatting tools
          pkgs.clang-tools
          pkgs.nodePackages.prettier
          pkgs.nodePackages.svgo
          pkgs.black
          pkgs.nixfmt
        ];
      };
    };
}
