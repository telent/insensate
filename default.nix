with import <nixpkgs> {};
let envs = import (builtins.fetchTarball {
      url= "https://github.com/nix-community/nix-environments/archive/master.tar.gz";
    }) { inherit pkgs; };
    arduino = envs.arduino;
in {
  test = stdenv.mkDerivation {
    name = "insensate";
    version = "0.1";
    src = ./.;
    doCheck = true;
  };
  inherit arduino;
}
