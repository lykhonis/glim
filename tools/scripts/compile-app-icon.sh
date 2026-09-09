#!/usr/bin/env bash
# Compile assets/glim-icon.png into an iOS Assets.car (and companion PNGs)
# or tvOS AppIcon.png files. Usage:
#   compile-app-icon.sh <icon.png> <iphoneos|iphonesimulator|appletvos|appletvsimulator> <min-os> <outdir>
set -euo pipefail

src="${1:?icon png}"
platform="${2:?platform}"
min_os="${3:?minimum deployment target}"
outdir="${4:?output directory}"

if [[ ! -f "$src" ]]; then
  echo "missing icon: $src" >&2
  exit 1
fi

mkdir -p "$outdir"
rm -rf "$outdir"/*

case "$platform" in
  appletvos|appletvsimulator)
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    # Flatten alpha so the TV icon is opaque, then letterbox to 1280x768 / 2560x1536.
    sips -s format jpeg -s formatOptions 100 "$src" --out "$tmp/flat.jpg" >/dev/null
    sips -s format png "$tmp/flat.jpg" --out "$tmp/opaque.png" >/dev/null
    sips -Z 768 "$tmp/opaque.png" --out "$tmp/fit.png" >/dev/null
    sips --padToHeightWidth 768 1280 --padColor 000000 "$tmp/fit.png" --out "$outdir/AppIcon.png" >/dev/null
    sips -Z 1536 "$tmp/opaque.png" --out "$tmp/fit2.png" >/dev/null
    sips --padToHeightWidth 1536 2560 --padColor 000000 "$tmp/fit2.png" --out "$outdir/AppIcon@2x.png" >/dev/null
    ;;
  iphoneos|iphonesimulator)
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    catalog="$tmp/AppIcon.xcassets"
    setdir="$catalog/AppIcon.appiconset"
    mkdir -p "$setdir"
    cp "$src" "$setdir/glim-icon.png"
    printf '%s\n' '{ "info": { "author": "xcode", "version": 1 } }' > "$catalog/Contents.json"
    cat > "$setdir/Contents.json" <<'JSON'
{
  "images" : [
    {
      "filename" : "glim-icon.png",
      "idiom" : "universal",
      "platform" : "ios",
      "size" : "1024x1024"
    }
  ],
  "info" : { "author": "xcode", "version": 1 }
}
JSON
    xcrun actool \
      --output-format human-readable-text \
      --notices --warnings \
      --platform "$platform" \
      --minimum-deployment-target "$min_os" \
      --app-icon AppIcon \
      --compile "$outdir" \
      --output-partial-info-plist "$outdir/icon-partial.plist" \
      "$catalog"
    ;;
  *)
    echo "unknown platform: $platform" >&2
    exit 1
    ;;
esac
