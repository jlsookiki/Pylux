#!/usr/bin/env bash
# Point the Flatpak manifest's pylux module at this checkout instead of the
# pinned upstream commit, so CI builds the code that was actually pushed.
# Fails if the manifest's shape changed and the pinned source survived,
# because a bundle built from the wrong commit is worse than no bundle.
#
# Usage: scripts/flatpak/use-local-checkout.sh [manifest]
set -euo pipefail

manifest="${1:-scripts/flatpak/io.github.ForWard_Technologies_LLC.Pylux.yml}"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

sed -e '/^  - name: pylux/,/^  - name:\|^$/{
  s|      - type: git|      - type: dir|
  s|        url:.*|        path: ../../|
  /commit:/d
}' "$manifest" > "$tmp"

module="$(awk '/^  - name: pylux$/{f=1; next} f && /^  - name:/{f=0} f' "$tmp")"
if [ -z "$module" ]; then
  echo "$0: no 'pylux' module found in $manifest" >&2
  exit 1
fi
if printf '%s\n' "$module" | grep -qE 'type: git|commit:|url:'; then
  echo "$0: pylux module still points at a git source after patching; manifest shape changed" >&2
  printf '%s\n' "$module" >&2
  exit 1
fi
if ! printf '%s\n' "$module" | grep -qE 'type: dir' || ! printf '%s\n' "$module" | grep -qE 'path: \.\./\.\./'; then
  echo "$0: pylux module was not switched to the local checkout" >&2
  printf '%s\n' "$module" >&2
  exit 1
fi

mv "$tmp" "$manifest"
trap - EXIT
echo "$manifest: pylux module now builds from the local checkout"
