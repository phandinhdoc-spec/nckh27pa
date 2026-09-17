#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p .build
set -x
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I. local_alert.cpp demo.cpp -o .build/demo
.build/demo
