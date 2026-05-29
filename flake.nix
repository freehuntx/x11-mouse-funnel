{
  description = "Lightweight X11 utility that solves the dead corner problem on multi-monitor setups";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:
    {
      nixosModules.default = ./nixos/modules/mouse-funnel.nix;
    }
    //
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        x11MouseFunnel = pkgs.stdenv.mkDerivation rec {
          pname = "x11-mouse-funnel";
          version = "0.1.0";
          name = "${pname}-${version}";

          src = ./.;

          nativeBuildInputs = [
            pkgs.gcc
            pkgs.git
          ];

          buildInputs = [
            pkgs.libx11
            pkgs.libxi
            pkgs.libxrandr
          ];

          buildPhase = ''
            gcc -O2 -o mouse_funnel mouse_funnel.c -lX11 -lXi -lXrandr
          '';

          installPhase = ''
            mkdir -p $out/bin
            install -Dm755 mouse_funnel $out/bin/mouse_funnel
          '';

          meta = with pkgs.lib; {
            description = "X11 utility that solves the dead corner problem on multi-monitor setups";
            homepage = "https://github.com/freehuntx/x11-mouse-funnel";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };
      in
      {
        packages.default = x11MouseFunnel;
        packages.x11-mouse-funnel = x11MouseFunnel;
      }
    );
}
