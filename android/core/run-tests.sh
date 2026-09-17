#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
KOTLINC=/home/pdd/.local/opt/nckh27pa/android-studio/plugins/Kotlin/kotlinc/bin/kotlinc
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
"$KOTLINC" src/*.kt tests/*.kt -include-runtime -d "$build/core.jar"
"$JAVA_HOME/bin/java" -cp "$build/core.jar" "${1:-core.CoreTestsKt}"
