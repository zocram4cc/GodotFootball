#!/usr/bin/env bash
# Runs the pinned Godot binary on the GodotFootball project, fetching it first if needed. A fresh
# checkout has no .godot/extension_list.cfg, without which the GDExtension is not loaded, so the
# project is imported once before the first run. A failed import stops here.
set -euo pipefail
GODOT=$("$(dirname "$0")/fetch_godot.sh")
PROJECT="$(cd "$(dirname "$0")/../godot" && pwd)"
# ponytail: Godot 4.5-4.7 can abort at shutdown of a headless import that loads a GDExtension: a
# deferred EditorHelp doc job runs after the docs are freed (godotengine/godot#111645).
# --quit-after gives the editor frames to run that job before it exits. Drop it when fixed upstream.
IMPORT_FRAMES=(--quit-after 300)
if [ ! -f "$PROJECT/.godot/extension_list.cfg" ]; then
  "$GODOT" --headless --path "$PROJECT" --import "${IMPORT_FRAMES[@]}" >/dev/null
fi
args=("$@")
# A caller that names a project keeps its own --path; a second one would break its relative paths.
[[ " $* " == *" --path "* ]] || args=(--path "$PROJECT" "${args[@]}")
[[ " $* " != *" --import "* ]] || args+=("${IMPORT_FRAMES[@]}")
exec "$GODOT" "${args[@]}"
