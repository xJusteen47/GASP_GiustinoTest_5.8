#!/usr/bin/env bash
# Sync the tracked pipeline files between this repo copy (source of truth) and the working folder that holds the data
# (Cascadeur scenes, sheets/, refs/). Only files that exist on both sides or are tracked here are copied; data is never touched.
#   ./sync.sh push   repo -> working folder   (after pulling the repo, or editing here)
#   ./sync.sh pull   working folder -> repo   (after a quick edit in the working folder; then review `git diff` and commit)
# Working folder: $ANIMPIPELINE_DIR, default <project>/Saved/AnimPipeline (this repo's Tools/AnimPipeline is <project>/Tools/AnimPipeline).
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
work="${ANIMPIPELINE_DIR:-$here/../../Saved/AnimPipeline}"
[ -d "$work" ] || { echo "working folder not found: $work (set ANIMPIPELINE_DIR)"; exit 1; }
cd "$here"
files=$(git ls-files . | grep -v '^\.gitignore$' | grep -v '^sync\.sh$')
case "${1:-}" in
  push) for f in $files; do cp "$here/$f" "$work/$f"; done; echo "pushed $(echo "$files" | wc -l) files to $work";;
  pull) for f in $files; do [ -f "$work/$f" ] && cp "$work/$f" "$here/$f"; done; echo "pulled from $work; review with: git diff --stat";;
  *) echo "usage: $0 push|pull"; exit 2;;
esac
