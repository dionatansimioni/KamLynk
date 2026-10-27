#!/usr/bin/env bash
# Builds a minimal libxml2.so.2 for the Sony SDK.
#
# libCr_Core.so links against libxml2.so.2 (the pre-2.14 ABI). Newer distros such as
# Ubuntu 26.04 only ship libxml2.so.16, so we bundle our own copy built without optional
# dependencies (ICU, lzma, zlib), which keeps the AppImage portable.
set -euo pipefail

VERSION=2.13.8
SHA256=277294cb33119ab71b2bc81f2f445e9bc9435b893ad15bb2cd2b0e859a0ee84a
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${LIBXML2_WORK:-$ROOT/build-deps/libxml2}"
OUT="${LIBXML2_OUT:-$ROOT/third_party/libxml2/lib}"

if [[ -f "$OUT/libxml2.so.2" ]]; then
    echo "libxml2.so.2 already built at $OUT"
    exit 0
fi

mkdir -p "$WORK" "$OUT"
cd "$WORK"
TARBALL="libxml2-$VERSION.tar.xz"
if [[ ! -f "$TARBALL" ]]; then
    curl -fsSL -o "$TARBALL" "https://download.gnome.org/sources/libxml2/${VERSION%.*}/$TARBALL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c -

rm -rf "src" "build"
mkdir src
tar -xf "$TARBALL" -C src --strip-components=1

cmake -S src -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DLIBXML2_WITH_ICU=OFF \
    -DLIBXML2_WITH_LZMA=OFF \
    -DLIBXML2_WITH_ZLIB=OFF \
    -DLIBXML2_WITH_ICONV=ON \
    -DLIBXML2_WITH_PYTHON=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF \
    -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_DEBUG=OFF \
    -DLIBXML2_WITH_HTTP=OFF \
    -DLIBXML2_WITH_FTP=OFF
cmake --build build

# Keep only the runtime library under its SONAME.
cp -L "build/libxml2.so.2" "$OUT/libxml2.so.2"
strip --strip-unneeded "$OUT/libxml2.so.2"
echo "Built $OUT/libxml2.so.2"
