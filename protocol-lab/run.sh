#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
KOTLINC="${KOTLINC:-/home/pdd/.local/opt/nckh27pa/android-studio/plugins/Kotlin/kotlinc/bin/kotlinc}"
JAVA_HOME="${JAVA_HOME:-/home/pdd/.local/opt/nckh27pa/android-studio/jbr}"
export JAVA_HOME
ln -sf "$JAVA_HOME/bin/java" build/java
"$KOTLINC" kotlin/*.kt -include-runtime -d build/kotlin-lab.jar
g++ -std=c++17 -Wall -Wextra -Werror -pedantic ${SANITIZE:+-fsanitize=address,undefined -fno-omit-frame-pointer -g -no-pie} cpp/*.cpp -o build/cpp-lab
# LeakSanitizer cannot operate under this host sandbox ptrace.
if [[ -n "${SANITIZE:-}" ]]; then
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
fi
python3 tests/run.py
