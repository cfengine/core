{
  description = "CFEngine Core";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    self.submodules = true;
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        baseVersion = lib.strings.trim (builtins.readFile ../../.CFVERSION);
        shortSha =
          if self ? rev then builtins.substring 0 9 self.rev
          else if self ? dirtyRev then builtins.substring 0 9 self.dirtyRev
          else "unknown0";
        version = "${baseVersion}a.${shortSha}";

        core = pkgs.callPackage ./core.nix { version = version; };
      in
      {
        packages = {
          default = core;
          buildTree = core.buildTree;
        };
      }
    );
}
