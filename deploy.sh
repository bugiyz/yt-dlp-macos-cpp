#!/bin/zsh
set -e

clang++ -std=c++20 -Wall -Wextra -pedantic main.cpp -o awesomeyt

mkdir -p ~/.local/bin

mv awesomeyt ~/.local/bin/awesomeyt
chmod +x ~/.local/bin/awesomeyt

hash -r 2>/dev/null

echo "awesomeyt updated successfully"
