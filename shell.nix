{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    cmake
    pkg-config
    libsodium
    iproute2
    iperf3
    iputils
    sysstat
    linuxPackages.perf
    python3
  ];
}
