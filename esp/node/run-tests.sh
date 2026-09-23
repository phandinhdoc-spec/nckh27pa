#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p .build
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -I../firmware/esp_node)
case "${1:-}" in
  '') ;;
  --sanitize) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g) ;;
  *) echo 'usage: run-tests.sh [--sanitize]' >&2; exit 2 ;;
esac
sources=(
  ../firmware/esp_node/mpu6050.cpp
  ../firmware/esp_node/ms5611.cpp
  ../firmware/esp_node/node_math.cpp
  ../firmware/esp_node/csv_row.cpp
)
tests=(mpu6050_test ms5611_test node_math_test csv_row_test)
# Compile from tests/ so the relative include path matches the host-test layout
# (arduino_i2c_bus.cpp and esp_node.ino are the only Arduino-only units and are excluded).
cd tests
set -x
for test in "${tests[@]}"; do
  g++ "${flags[@]}" "${sources[@]}" "${test}.cpp" -o "../.build/${test}"
done
for test in "${tests[@]}"; do
  "../.build/${test}"
done
