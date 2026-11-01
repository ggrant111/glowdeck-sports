#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude src/GameCore.cpp simulator/main.cpp -o .build/simulator
.build/simulator
if command -v pio >/dev/null; then
  pio test -e native
else
  echo 'PlatformIO not on PATH. Run `pio test -e native` after installing it.'
fi
