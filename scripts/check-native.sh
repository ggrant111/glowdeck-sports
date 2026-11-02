#!/usr/bin/env sh
set -eu
mkdir -p .build
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude src/GameCore.cpp simulator/main.cpp -o .build/simulator
.build/simulator
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude src/GameCore.cpp src/GoalEngine.cpp simulator/v02.cpp -o .build/game-v02
.build/game-v02
