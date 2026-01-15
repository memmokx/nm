{
  pkgs ? import <nixpkgs> { },
}:

with pkgs;
let
  llvm = llvmPackages_21;
  darwinPackages = lib.optionals stdenv.isDarwin [
    elf-header-real
  ];
in
mkShell.override { stdenv = llvm.stdenv; } {
  packages = [
    bear
    clang-tools
  ]
  ++ darwinPackages;
}
