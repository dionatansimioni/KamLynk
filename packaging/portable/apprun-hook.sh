# Sourced by the AppImage's AppRun before starting KamLynk.

# The Sony Camera Remote SDK binaries need glibc 2.34+; explain instead of a loader error.
kamlynk_glibc="$(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
kamlynk_glibc="${kamlynk_glibc#glibc }"
if [ -n "$kamlynk_glibc" ] && [ "$(printf '%s\n2.34\n' "$kamlynk_glibc" | sort -V | head -n1)" != "2.34" ]; then
    kamlynk_msg="KamLynk precisa de um Linux com glibc 2.34 ou mais nova (este sistema tem $kamlynk_glibc). Distros de 2022 em diante: Ubuntu 22.04+, Debian 12+, Fedora 35+, RHEL/Rocky/Alma 9+."
    echo "$kamlynk_msg" >&2
    if command -v notify-send >/dev/null 2>&1; then notify-send -i dialog-error "KamLynk" "$kamlynk_msg" || true; fi
    exit 1
fi

# Fallback libraries live in usr/optional/<soname>/ and are used only when the system lacks
# them: a system copy is always preferred (it matches the rest of the desktop and is newer).
kamlynk_libdirs="/usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib /lib64 /lib/x86_64-linux-gnu"
kamlynk_host_lib() { # $1 = soname, prints its path when the system has it
    for d in $kamlynk_libdirs; do
        if [ -e "$d/$1" ]; then echo "$d/$1"; return 0; fi
    done
    for ldc in ldconfig /sbin/ldconfig /usr/sbin/ldconfig; do
        p="$($ldc -p 2>/dev/null | grep -m1 " $1 " | sed 's/.*=> //')" || true
        if [ -n "$p" ]; then echo "$p"; return 0; fi
    done
    return 1
}
kamlynk_extra=""
for kamlynk_dir in "$APPDIR"/usr/optional/*/; do
    [ -d "$kamlynk_dir" ] || continue
    kamlynk_so="$(basename "$kamlynk_dir")"
    kamlynk_host="$(kamlynk_host_lib "$kamlynk_so" || true)"
    if [ "$kamlynk_so" = "libstdc++.so.6" ]; then
        # The SDK needs GLIBCXX_3.4.30 (GCC 12+): use ours only on older systems.
        if [ -n "$kamlynk_host" ] && grep -qa "GLIBCXX_3.4.30" "$kamlynk_host"; then continue; fi
    elif [ -n "$kamlynk_host" ]; then
        continue
    fi
    kamlynk_extra="${kamlynk_extra:+$kamlynk_extra:}${kamlynk_dir%/}"
done
if [ -n "$kamlynk_extra" ]; then
    export LD_LIBRARY_PATH="$kamlynk_extra${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
unset -f kamlynk_host_lib
unset kamlynk_glibc kamlynk_msg kamlynk_libdirs kamlynk_extra kamlynk_dir kamlynk_so kamlynk_host
