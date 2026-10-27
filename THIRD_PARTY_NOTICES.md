# Third-party notices / Avisos de terceiros

KamLynk's own source code is released under the [MIT License](LICENSE). The KamLynk AppImage
also contains the third-party components below, each under its own license. The full license
texts are in [`LICENSES/`](LICENSES) and, inside the AppImage, in `usr/share/doc/kamlynk/`.

O código do KamLynk é distribuído sob a [licença MIT](LICENSE). O AppImage também traz os
componentes de terceiros abaixo, cada um com a sua licença. Os textos completos estão em
[`LICENSES/`](LICENSES) e, dentro do AppImage, em `usr/share/doc/kamlynk/`.

## Sony Camera Remote SDK (v2.02.00)

`libCr_Core.so`, `CrAdapter/libCr_PTP_IP.so`, `CrAdapter/libCr_PTP_USB.so`

Copyright © Sony Group Corporation. Proprietary; used and distributed inside the KamLynk
application under the [Camera Remote SDK License Agreement](https://support.d-imaging.sony.co.jp/app/sdk/licenseagreement/en.html).
The SDK is **not** part of this repository and is **not** covered by KamLynk's MIT license.
To build KamLynk, download the SDK from Sony and accept its license
(see [README](README.md#building-from-source)).

End-user terms of the KamLynk AppImage, required by that agreement (also shown on first start):

- You may copy and use the application solely to remotely control or use the camera in normal use.
- While controlled by this application, the camera may be out of the manufacturer's warranty,
  as separately specified by Sony (this does not apply where local law does not allow it).
- The application must not be used for military or weapons-related purposes, nor resold or
  transferred to anyone who intends to.
- Sony is not the owner, licensor, creator or developer of KamLynk, and does not support it.

Sony, ZV-E10 and related names are trademarks or registered trademarks of Sony Group
Corporation or its affiliates. KamLynk is an independent project, not affiliated with,
endorsed or supported by Sony.

## Qt 6.10.3

Qt Core, Gui, Qml, Quick, Quick Controls 2, Svg, Wayland and Network modules and plugins.

Copyright © The Qt Company Ltd. and other contributors.
License: GNU Lesser General Public License v3 ([LGPL-3.0](LICENSES/LGPL-3.0.txt), which
incorporates the [GPL-3.0](LICENSES/GPL-3.0.txt)). Qt is linked dynamically: the Qt libraries
are separate files inside the AppImage (`./KamLynk-*.AppImage --appimage-extract`, see
`squashfs-root/usr/lib`) and can be replaced by compatible builds. Source code:
<https://download.qt.io/official_releases/qt/6.10/6.10.3/>.

## QtKeychain 0.15.0

Copyright © 2011-2015 Frank Osterfeld and contributors.
License: [BSD-3-Clause](LICENSES/QtKeychain-BSD-3-Clause.txt).
Source: <https://github.com/frankosterfeld/qtkeychain>.

## libxml2 2.13.8

Copyright © 1998-2012 Daniel Veillard and contributors.
License: [MIT](LICENSES/libxml2-MIT.txt). Built from the upstream release by
`scripts/build-libxml2.sh`. Source: <https://gitlab.gnome.org/GNOME/libxml2>.

## libssh2 1.10.0 (shipped with the Sony SDK, `CrAdapter/libssh2.so`)

Copyright © 2004-2007 Sara Golemon, © 2005-2006 Mikhail Gusarov, © 2006-2007 The Written
Word, Inc., © 2007 Eli Fant, © 2009-2021 Daniel Stenberg, © 2008-2009 Simon Josefsson,
© 2000 Markus Friedl, © 2015 Microsoft Corp. License: [BSD-3-Clause](LICENSES/libssh2-BSD-3-Clause.txt).

## OpenSSL 3.0.8 (statically linked into the Sony SDK's `libssh2.so`)

Copyright © 1998-2022 The OpenSSL Project.
License: [Apache License 2.0](LICENSES/OpenSSL-Apache-2.0.txt).

## libusb 1.0.26 (shipped with the Sony SDK, `CrAdapter/libusb-1.0.so`)

Copyright © 2001 Johannes Erdfelt, © 2007-2009 Daniel Drake, © 2010-2012 Peter Stuge,
© 2008-2016 Nathan Hjelm, © 2009-2013 Pete Batard, © 2009-2013 Ludovic Rousseau,
© 2010-2012 Michael Plante, © 2011-2013 Hans de Goede, © 2012-2013 Martin Pieuchot,
© 2012-2013 Toby Gray, © 2013-2018 Chris Dickens and the other contributors listed in its
`AUTHORS` file.
License: GNU Lesser General Public License v2.1 ([LGPL-2.1](LICENSES/libusb-LGPL-2.1.txt)).
The library is a separate, replaceable file in the AppImage (`usr/bin/CrAdapter/`). The
corresponding source code, as provided by Sony with the SDK, is attached to every
[KamLynk release](https://github.com/dionatansimioni/KamLynk/releases)
(`libusb-1.0.26-source.zip`); upstream: <https://libusb.info>.

## GCC runtime (libstdc++, libgcc_s) 14.2.0

Bundled as a fallback for systems with an older C++ runtime.
Copyright © Free Software Foundation, Inc. License: [GPL-3.0](LICENSES/GPL-3.0.txt) with the
[GCC Runtime Library Exception 3.1](LICENSES/GCC-exception-3.1.txt).

## System libraries (fallbacks)

When missing on the host, the AppImage uses its own copy of GLib, libsecret, libgcrypt,
libgpg-error, Kerberos (MIT krb5), libkeyutils, OpenSSL `libcrypto`, libffi, PCRE2,
util-linux `libmount`/`libblkid` and libselinux, taken unmodified from AlmaLinux 9. Each is
distributed under its own license (LGPL-2.1+, MIT, BSD-style, Apache-2.0 or public domain);
sources: <https://git.almalinux.org/rpms>.
