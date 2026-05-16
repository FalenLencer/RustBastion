#!/bin/bash
# Lance RUST BASTION depuis le répertoire du projet
cd "$(dirname "$0")"

# Audio WSLg (Windows Subsystem for Linux)
if [ -S /mnt/wslg/runtime-dir/pulse/native ]; then
  export PULSE_SERVER=unix:/mnt/wslg/runtime-dir/pulse/native
fi

exec ./build/rustbastion "$@"
