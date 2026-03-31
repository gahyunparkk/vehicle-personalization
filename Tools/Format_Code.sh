#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$ROOT_DIR/Ecu_Safety_State_TC375"

cd "$SCRIPT_DIR"

TARGETS=()

for dir in "$PROJECT_DIR/AppSw" "$PROJECT_DIR/BaseSw"; do
    if [ -d "$dir" ]; then
        while IFS= read -r -d '' file; do
            TARGETS+=("$file")
        done < <(find "$dir" -type f -name "*.c" -print0)
    fi
done

for file in \
    "$PROJECT_DIR/Cpu0_Main.c" \
    "$PROJECT_DIR/Cpu1_Main.c" \
    "$PROJECT_DIR/Cpu2_Main.c"
do
    if [ -f "$file" ]; then
        TARGETS+=("$file")
    fi
done

if [ "${#TARGETS[@]}" -eq 0 ]; then
    echo "No target .c files found."
    exit 0
fi

echo "Targets:"
for file in "${TARGETS[@]}"; do
    printf '  %s\n' "${file#$ROOT_DIR/}"
done

read -r -p "Format these files with clang-format? [y/N] " answer

if [[ "$answer" =~ ^[Yy]$ ]]; then
    clang-format -i --style=file "${TARGETS[@]}"
    echo "Formatting done."
else
    echo "Cancelled."
fi