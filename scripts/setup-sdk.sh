#!/usr/bin/env bash
# Copies the Sony Camera Remote SDK headers and libraries into third_party/crsdk/.
#
# Sony's license does not allow redistributing the SDK, so it is not part of this
# repository. Download "Camera Remote SDK" for Linux 64-bit (x86_64) from
# https://support.d-imaging.sony.co.jp/app/sdk/en/index.html, accept Sony's license and run:
#
#   ./scripts/setup-sdk.sh ~/Downloads/CrSDK_v2.02.00_*_Linux64PC.zip
#
# Without an argument, the CrSDK_*.zip in the repository root is used.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/crsdk"
EXPECTED_VERSION="v2.02.00"

ZIP="${1:-}"
if [[ -z "$ZIP" ]]; then
    shopt -s nullglob
    candidates=("$ROOT"/CrSDK_*_Linux64PC.zip)
    if [[ ${#candidates[@]} -ne 1 ]]; then
        echo "Uso: $0 <CrSDK_..._Linux64PC.zip>" >&2
        echo "Baixe o SDK em https://support.d-imaging.sony.co.jp/app/sdk/en/index.html" >&2
        exit 1
    fi
    ZIP="${candidates[0]}"
fi
[[ -f "$ZIP" ]] || { echo "Arquivo não encontrado: $ZIP" >&2; exit 1; }
[[ "$(basename "$ZIP")" == *"$EXPECTED_VERSION"* ]] ||
    echo "Aviso: o KamLynk foi testado com o SDK $EXPECTED_VERSION; $(basename "$ZIP") pode não ser compatível." >&2

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# The headers and libraries ship inside the RemoteCli sample.
unzip -q -o "$ZIP" RemoteCli.zip -d "$WORK"
unzip -q -o "$WORK/RemoteCli.zip" 'app/CRSDK/*' 'external/crsdk/*' -d "$WORK/cli"

rm -rf "$DEST"
mkdir -p "$DEST/include" "$DEST/lib"
cp "$WORK"/cli/app/CRSDK/*.h "$DEST/include/"
cp -r "$WORK"/cli/external/crsdk/. "$DEST/lib/"

for f in include/CameraRemote_SDK.h lib/libCr_Core.so lib/CrAdapter/libCr_PTP_IP.so lib/CrAdapter/libCr_PTP_USB.so; do
    [[ -f "$DEST/$f" ]] || { echo "SDK incompleto: falta $f" >&2; exit 1; }
done
echo "SDK da Sony pronto em $DEST"
