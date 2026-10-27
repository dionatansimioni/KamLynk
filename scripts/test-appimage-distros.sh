#!/usr/bin/env bash
# Runs the AppImage in clean containers of many distros (with the libraries every desktop
# has: OpenGL, fontconfig, X11/Wayland client, D-Bus, GLib). For each one: SDK self-check
# (loads libCr_Core, CrAdapter plugins, libssh2, libxml2) and the full QML UI in demo mode.
# Distros older than glibc 2.34 must refuse to start with a clear message.
#   scripts/test-appimage-distros.sh [dist/KamLynk-x.y-x86_64.AppImage] [image...]
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APPIMAGE="$(readlink -f "${1:-$(ls -t "$ROOT"/dist/KamLynk-*-x86_64.AppImage | head -n1)}")"
shift || true
IMAGES=("$@")
[[ ${#IMAGES[@]} -gt 0 ]] || IMAGES=(
    ubuntu:22.04 ubuntu:24.04 ubuntu:26.04 debian:12 debian:13 debian:testing
    fedora:latest almalinux:9 archlinux:latest opensuse/tumbleweed
    ubuntu:20.04)
# RHEL/Rocky/Alma 10 need an x86-64-v3 CPU (AVX2) even to start a container; add
# rockylinux/rockylinux:10 on machines that have one.

deps() {
    case "$1" in
    ubuntu:*|debian:*) echo 'export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq libgl1 libegl1 libfontconfig1 libxkbcommon0 libdbus-1-3 "libglib2.0-0t64|libglib2.0-0" libx11-6 libx11-xcb1 libxcb1 libwayland-client0 libwayland-egl1 libwayland-cursor0 >/dev/null 2>&1 || apt-get install -y -qq libgl1 libegl1 libfontconfig1 libxkbcommon0 libdbus-1-3 libglib2.0-0 libx11-6 libx11-xcb1 libxcb1 libwayland-client0 libwayland-egl1 libwayland-cursor0 >/dev/null' ;;
    fedora:*|almalinux:*|rockylinux*) echo 'dnf -y -q install mesa-libGL mesa-libEGL fontconfig libxkbcommon dbus-libs glib2 libX11 libX11-xcb libxcb libwayland-client libwayland-egl libwayland-cursor >/dev/null' ;;
    archlinux:*) echo 'pacman -Sy --noconfirm --needed mesa fontconfig libxkbcommon dbus glib2 libx11 libxcb wayland >/dev/null' ;;
    opensuse/*) echo 'zypper -q -n in Mesa-libGL1 Mesa-libEGL1 fontconfig libxkbcommon0 libdbus-1-3 libglib-2_0-0 libgio-2_0-0 libX11-6 libX11-xcb1 libxcb1 libwayland-client0 libwayland-egl1 libwayland-cursor0 >/dev/null' ;;
    esac
}

pass=0; fail=0; summary=()
for image in "${IMAGES[@]}"; do
    out="$(docker run --rm -v "$APPIMAGE:/app/KamLynk.AppImage:ro" -e QT_QPA_PLATFORM=offscreen \
        -e QT_QUICK_BACKEND=software -e APPIMAGE_EXTRACT_AND_RUN=1 "$image" sh -c "
        $(deps "$image") || { echo 'DEPS-FAILED'; exit 3; }
        glibc=\$(getconf GNU_LIBC_VERSION)
        cd /tmp && /app/KamLynk.AppImage --sdk-check 2>&1; echo \"sdk-exit=\$?\"
        /app/KamLynk.AppImage --demo --screenshot /tmp/shot.png >/tmp/ui.log 2>&1; echo \"ui-exit=\$?\"
        [ -s /tmp/shot.png ] && echo shot-ok; grep -iE 'error|cannot|not found' /tmp/ui.log | head -n3
        echo \"\$glibc\"" 2>&1)"
    glibc="$(echo "$out" | grep -o 'glibc [0-9.]*' | tail -n1)"
    if echo "$out" | grep -q 'precisa de um Linux com glibc 2.34'; then
        result="recusado com mensagem clara (esperado em glibc < 2.34)"
        [[ "$glibc" < "glibc 2.34" ]] && pass=$((pass+1)) || fail=$((fail+1))
    elif echo "$out" | grep -q 'init: ok' && echo "$out" | grep -q 'sdk-exit=0' \
         && echo "$out" | grep -q 'ui-exit=0' && echo "$out" | grep -q shot-ok; then
        result="OK (SDK + interface)"; pass=$((pass+1))
    else
        result="FALHOU"; fail=$((fail+1))
        echo "----- $image -----"; echo "$out" | tail -n 15
    fi
    summary+=("$(printf '%-28s %-12s %s' "$image" "$glibc" "$result")")
done
printf '%s\n' "${summary[@]}"
echo "passaram: $pass  falharam: $fail"
[[ $fail -eq 0 ]]
