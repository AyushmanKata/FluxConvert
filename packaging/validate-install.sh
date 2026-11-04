#!/usr/bin/env bash
set -euo pipefail

prefix="${1:?usage: $0 <install-prefix>}"

test -x "$prefix/bin/fluxconvert-cli"
test -x "$prefix/bin/fluxconvert-gui"
test -f "$prefix/share/applications/org.fluxconvert.FluxConvert.desktop"
test -f "$prefix/share/icons/hicolor/scalable/apps/org.fluxconvert.FluxConvert.svg"
test -f "$prefix/share/metainfo/org.fluxconvert.FluxConvert.metainfo.xml"
test -f "$prefix/share/doc/fluxconvert/LICENSE"
test -f "$prefix/share/doc/fluxconvert/THIRD_PARTY_NOTICES.md"

"$prefix/bin/fluxconvert-cli" --help >/dev/null
echo "FluxConvert installation validation passed: $prefix"
