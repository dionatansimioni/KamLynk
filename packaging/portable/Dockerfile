# Portable AppImage build environment: glibc 2.34 (AlmaLinux 9, manylinux_2_34) — the floor
# set by the Sony SDK binaries (GLIBC_2.34) — with GCC 14 (gcc-toolset), the official Qt
# binaries (also glibc 2.34) and QtKeychain built against them. Everything compiled here
# runs on any distro with glibc >= 2.34: Ubuntu 22.04+, Debian 12+, RHEL/Alma/Rocky 9+,
# Fedora 35+, openSUSE Leap 16+/Tumbleweed, Arch...
FROM quay.io/pypa/manylinux_2_34_x86_64:latest

ARG QT_VERSION=6.10.3
ARG QTKEYCHAIN_VERSION=0.15.0
# libstdc++ 14 built for glibc 2.17 (conda-forge): the SDK needs GLIBCXX_3.4.30 (GCC 12+)
# but AlmaLinux 9 ships GCC 11's. Bundled in the AppImage, used only on older hosts.
ARG LIBSTDCXX_PKG=libstdcxx-14.2.0-h8f9b012_2.conda
ARG LIBSTDCXX_SHA256=8f5bd92e4a24e1d35ba015c5252e8f818898478cb3bc50bd8b12ab54707dc4da
ARG LIBGCC_PKG=libgcc-14.2.0-h767d61c_2.conda
ARG LIBGCC_SHA256=3a572d031cb86deb541d15c1875aaa097baefc0c580b54dc61f5edab99215792

RUN dnf -y install \
        ninja-build patchelf file zstd unzip which \
        mesa-libGL-devel mesa-libEGL-devel libglvnd-devel \
        fontconfig-devel freetype-devel wayland-devel dbus-devel glib2-devel libsecret-devel \
        libxkbcommon-x11 xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm \
    && dnf clean all

RUN /opt/python/cp312-cp312/bin/pip install --root-user-action=ignore -q aqtinstall \
    && /opt/python/cp312-cp312/bin/aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 -O /opt/qt \
    && ln -s /opt/qt/${QT_VERSION}/gcc_64 /opt/qt/current

RUN curl -fsSL -o /tmp/qtkeychain.tar.gz \
        https://github.com/frankosterfeld/qtkeychain/archive/refs/tags/${QTKEYCHAIN_VERSION}.tar.gz \
    && mkdir /tmp/qtkeychain && tar -xf /tmp/qtkeychain.tar.gz -C /tmp/qtkeychain --strip-components=1 \
    && cmake -S /tmp/qtkeychain -B /tmp/qtkeychain/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF -DBUILD_TEST_APPLICATION=OFF -DLIBSECRET_SUPPORT=ON \
        -DCMAKE_PREFIX_PATH=/opt/qt/current -DCMAKE_INSTALL_PREFIX=/opt/qtkeychain -DCMAKE_INSTALL_LIBDIR=lib \
    && cmake --build /tmp/qtkeychain/build && cmake --install /tmp/qtkeychain/build \
    && rm -rf /tmp/qtkeychain /tmp/qtkeychain.tar.gz

RUN mkdir -p /opt/libstdcxx /tmp/conda && cd /tmp/conda \
    && for pkg in "${LIBSTDCXX_PKG} ${LIBSTDCXX_SHA256}" "${LIBGCC_PKG} ${LIBGCC_SHA256}"; do \
         set -- $pkg; \
         curl -fsSL -o "$1" "https://conda.anaconda.org/conda-forge/linux-64/$1"; \
         echo "$2  $1" | sha256sum -c -; \
         unzip -q -o "$1" 'pkg-*.tar.zst'; \
         tar --zstd -xf pkg-*.tar.zst; rm -f pkg-*.tar.zst; \
       done \
    && cp -L lib/libstdc++.so.6 lib/libgcc_s.so.1 /opt/libstdcxx/ \
    && rm -rf /tmp/conda

ENV QT_DIR=/opt/qt/current \
    CMAKE_PREFIX_PATH=/opt/qt/current:/opt/qtkeychain \
    LIBSTDCXX_DIR=/opt/libstdcxx
