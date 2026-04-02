#!/usr/bin/env bash
set -uo pipefail

current_branch="$(git branch --show-current)"
failed_branches=()

git fetch --all --prune

git for-each-ref --format='%(refname:short)' refs/heads | while read -r lb; do
    upstream="$(git for-each-ref --format='%(upstream:short)' "refs/heads/$lb")"

    if [[ -z "$upstream" ]]; then
        echo "[skip] no upstream: $lb"
        continue
    fi

    echo "[update] $lb <- $upstream"

    if ! git switch "$lb" >/dev/null 2>&1; then
        echo "[fail] switch failed: $lb"
        failed_branches+=("$lb")
        continue
    fi

    if ! git pull --ff-only; then
        echo "[fail] pull failed: $lb"
        failed_branches+=("$lb")
        continue
    fi
done

git switch "$current_branch" >/dev/null 2>&1 || true

echo "[done] returned to $current_branch"
if [[ ${#failed_branches[@]} -gt 0 ]]; then
    echo "[failed branches]"
    printf ' - %s\n' "${failed_branches[@]}"
fi