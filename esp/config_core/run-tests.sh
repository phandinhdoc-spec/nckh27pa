#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p .build
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -I.)
case "${1:-}" in
  '') ;;
  --sanitize) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g) ;;
  *) echo 'usage: run-tests.sh [--sanitize]' >&2; exit 2 ;;
esac
set -x
g++ "${flags[@]}" config_core.cpp tests/config_core_test.cpp -o .build/tests
.build/tests
