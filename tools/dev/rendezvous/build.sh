#!/bin/sh
# Builds rv.dll (the rendezvous hook DLL) and its unload regression test with the mingw-w64 cross compiler.
# Usage: sh build.sh OUT_DIR
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?usage: build.sh OUT_DIR}"
mkdir -p "$out"
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -fno-math-errno -shared -static -o "$out/rv.dll" "$here/rv.c" -lkernel32
x86_64-w64-mingw32-gcc -O2 -o "$out/rv_pin_test.exe" "$here/rv_pin_test.c"
echo "built $out/rv.dll $out/rv_pin_test.exe"
