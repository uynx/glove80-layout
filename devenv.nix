{ pkgs, ... }:
{
  env.NIX_ENFORCE_NO_NATIVE = "0";
  packages = [
    pkgs.gcc
    pkgs.gnumake
    pkgs.git
    pkgs.curl
    pkgs.poppler-utils
    (pkgs.python3.withPackages (p: [ p.pyarrow p.fsspec p.aiohttp ]))
  ];
}
