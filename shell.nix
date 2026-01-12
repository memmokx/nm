{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell.override { stdenv = pkgs.llvmPackages_21.stdenv; } {
  packages = [
    pkgs.llvmPackages_21.libcxx
    pkgs.clang-tools
    pkgs.bear
  ];
  buildInputs = [
    (pkgs.writeScriptBin "bmake" ''
      #!${pkgs.stdenv.shell}
      exec ${pkgs.bear}/bin/bear -- make "$@"
    '')
  ];
}
