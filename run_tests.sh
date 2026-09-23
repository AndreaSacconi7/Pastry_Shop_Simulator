#!/usr/bin/env bash
# Builds the simulator and runs every public test case, diffing against the expected output.
set -u

CC=${CC:-gcc}
BIN=./proapi
TESTS_DIR=tests

echo "building with $CC..."
$CC -Wall -Werror -std=gnu11 -O2 main.c -o "$BIN" -lm || exit 1

pass=0
fail=0

for input in "$TESTS_DIR"/*.txt; do
    case "$input" in *.output.txt) continue ;; esac

    expected="${input%.txt}.output.txt"
    [ -f "$expected" ] || continue

    name=$(basename "$input" .txt)
    start=$(date +%s)
    actual=$("$BIN" < "$input")
    elapsed=$(( $(date +%s) - start ))

    if [ "$actual" = "$(cat "$expected")" ]; then
        printf 'PASS  %-12s %3ss\n' "$name" "$elapsed"
        pass=$((pass + 1))
    else
        printf 'FAIL  %-12s %3ss\n' "$name" "$elapsed"
        fail=$((fail + 1))
    fi
done

echo "---"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
