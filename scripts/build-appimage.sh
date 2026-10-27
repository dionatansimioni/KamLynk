#!/usr/bin/env bash
# Builds dist/KamLynk-<version>-x86_64.AppImage that runs on any x86_64 distro with
# glibc >= 2.34 (the Sony SDK's floor): compiles inside the glibc 2.34 container from
# packaging/portable/Dockerfile, runs the tests there, then packages with the static
# AppImage runtime (no libfuse2 needed on the host).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="kamlynk-portable-build:qt6.10.3"

docker build -q -t "$IMAGE" "$ROOT/packaging/portable" >/dev/null
docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" /src/scripts/portable-build-inside.sh
ls -la "$ROOT"/dist/KamLynk-*.AppImage
