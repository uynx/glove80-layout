{ pkgs, ... }:
{
  env.NIX_ENFORCE_NO_NATIVE = "0";
  packages = [
    pkgs.gcc
    pkgs.gnumake
    pkgs.poppler-utils
    pkgs.python3
    pkgs.dash
  ];
}
