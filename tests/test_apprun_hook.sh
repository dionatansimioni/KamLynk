#!/bin/sh
# Checks packaging/portable/apprun-hook.sh: a fallback library is added to the library path
# only when the system lacks it, and libstdc++ only when the system's is older than GCC 12.
set -eu
HOOK="$1"
APPDIR="$(mktemp -d)"
trap 'rm -rf "$APPDIR"' EXIT
mkdir -p "$APPDIR/usr/optional/libkamlynk-missing.so.1" "$APPDIR/usr/optional/libc.so.6" \
         "$APPDIR/usr/optional/libstdc++.so.6"
export APPDIR
LD_LIBRARY_PATH=/previous
export LD_LIBRARY_PATH

. "$HOOK"

case "$LD_LIBRARY_PATH" in
*/usr/optional/libkamlynk-missing.so.1*) ;;
*) echo "FAIL: missing library not added: $LD_LIBRARY_PATH"; exit 1 ;;
esac
case "$LD_LIBRARY_PATH" in
*/usr/optional/libc.so.6*) echo "FAIL: system library shadowed: $LD_LIBRARY_PATH"; exit 1 ;;
esac
case "$LD_LIBRARY_PATH" in
*:/previous) ;;
*) echo "FAIL: previous LD_LIBRARY_PATH lost: $LD_LIBRARY_PATH"; exit 1 ;;
esac
# libstdc++: ours only when the system's lacks GLIBCXX_3.4.30 (GCC 12+).
system_new=no
for lib in /usr/lib64/libstdc++.so.6 /usr/lib/x86_64-linux-gnu/libstdc++.so.6 /usr/lib/libstdc++.so.6; do
    if [ -e "$lib" ] && grep -qa GLIBCXX_3.4.30 "$lib"; then system_new=yes; fi
done
case "$system_new:$LD_LIBRARY_PATH" in
yes:*/usr/optional/libstdc++.so.6*) echo "FAIL: newer system libstdc++ shadowed"; exit 1 ;;
no:*/usr/optional/libstdc++.so.6*) ;;
no:*) echo "FAIL: old system libstdc++ but ours not used"; exit 1 ;;
esac
echo "apprun hook: OK ($LD_LIBRARY_PATH)"
