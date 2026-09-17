#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
compiler=/home/pdd/.local/opt/nckh27pa/android-studio/plugins/Kotlin/kotlinc/bin/kotlinc
libs="$PWD/.tools/gradle-8.13/lib"
build_dir=$(mktemp -d /tmp/and-001b.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
"$compiler" android/core/src/*.kt android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt android/app/src/test/java/vn/nckh27pa/fallsafe/DemoTests.kt -cp "$libs/junit-4.13.2.jar" -include-runtime -d "$build_dir/tests.jar"
"$JAVA_HOME/bin/java" -cp "$build_dir/tests.jar:$libs/junit-4.13.2.jar:$libs/hamcrest-core-1.3.jar" org.junit.runner.JUnitCore vn.nckh27pa.fallsafe.DemoTests
