#!/usr/bin/env bash
# Package a NativeActivity APK. Usage:
#   package-android-apk.sh <lib.so> <AndroidManifest.xml> <icon.png> <out.apk> <sdk> <abi>
set -euo pipefail

lib="${1:?native shared library}"
manifest="${2:?AndroidManifest.xml}"
icon="${3:?app icon png}"
out="${4:?output apk}"
sdk="${5:?Android SDK root}"
abi="${6:-arm64-v8a}"

if [[ ! -f "$lib" ]]; then
    echo "missing library: $lib" >&2
    exit 1
fi
if [[ ! -f "$manifest" ]]; then
    echo "missing manifest: $manifest" >&2
    exit 1
fi
if [[ ! -f "$icon" ]]; then
    echo "missing icon: $icon" >&2
    exit 1
fi

build_tools=""
if [[ -d "$sdk/build-tools" ]]; then
    build_tools="$(ls -1 "$sdk/build-tools" | sort | tail -n 1)"
fi
if [[ -z "$build_tools" ]]; then
    echo "no Android build-tools in $sdk/build-tools; skipping APK" >&2
    exit 0
fi
bt="$sdk/build-tools/$build_tools"
aapt2="$bt/aapt2"
zipalign="$bt/zipalign"
apksigner="$bt/apksigner"
if [[ ! -x "$aapt2" || ! -x "$zipalign" || ! -x "$apksigner" ]]; then
    echo "build-tools $build_tools missing aapt2/zipalign/apksigner; skipping APK" >&2
    exit 0
fi

android_jar=""
if [[ -d "$sdk/platforms" ]]; then
    for api in android-35 android-34 android-33 android-32 android-31 android-30; do
        if [[ -f "$sdk/platforms/$api/android.jar" ]]; then
            android_jar="$sdk/platforms/$api/android.jar"
            break
        fi
    done
fi
if [[ -z "$android_jar" ]]; then
    echo "no android.jar in $sdk/platforms; skipping APK" >&2
    exit 0
fi

workdir="$(mktemp -d "${TMPDIR:-/tmp}/glim-apk.XXXXXX")"
cleanup() { rm -rf "$workdir"; }
trap cleanup EXIT

res="$workdir/res"
mkdir -p "$res/mipmap-mdpi" "$res/mipmap-hdpi" "$res/mipmap-xhdpi" "$res/mipmap-xxhdpi" \
    "$res/mipmap-xxxhdpi" "$res/drawable"

resize_png() {
    local src="$1" dest="$2" w="$3" h="$4"
    if command -v sips >/dev/null 2>&1; then
        sips -z "$h" "$w" "$src" --out "$dest" >/dev/null
    elif command -v magick >/dev/null 2>&1; then
        magick "$src" -resize "${w}x${h}" "$dest"
    elif command -v convert >/dev/null 2>&1; then
        convert "$src" -resize "${w}x${h}" "$dest"
    else
        cp "$src" "$dest"
    fi
}

resize_png "$icon" "$res/mipmap-mdpi/ic_launcher.png" 48 48
resize_png "$icon" "$res/mipmap-hdpi/ic_launcher.png" 72 72
resize_png "$icon" "$res/mipmap-xhdpi/ic_launcher.png" 96 96
resize_png "$icon" "$res/mipmap-xxhdpi/ic_launcher.png" 144 144
resize_png "$icon" "$res/mipmap-xxxhdpi/ic_launcher.png" 192 192
resize_png "$icon" "$res/drawable/banner.png" 320 180

compiled="$workdir/compiled"
mkdir -p "$compiled"
"$aapt2" compile --dir "$res" -o "$compiled"

flats=()
while IFS= read -r flat; do
    flats+=("$flat")
done <<EOF
$(find "$compiled" -name '*.flat' | sort)
EOF
if [[ ${#flats[@]} -eq 0 ]]; then
    echo "aapt2 compile produced no resources" >&2
    exit 1
fi

link="$workdir/linked.apk"
"$aapt2" link -o "$link" -I "$android_jar" --manifest "$manifest" --auto-add-overlay "${flats[@]}"

# Keep aapt2's uncompressed resources.arsc and add the native lib stored
# uncompressed so zipalign can 4-byte-align .arsc and page-align .so files.
# Re-zipping the whole APK with default compression fails install on API 30+:
# "resources.arsc of installed APKs to be stored uncompressed and aligned
# on a 4-byte boundary".
unsigned="$workdir/unsigned.apk"
cp "$link" "$unsigned"
mkdir -p "$workdir/libadd/lib/$abi"
cp "$lib" "$workdir/libadd/lib/$abi/libglim-hello.so"
(
    cd "$workdir/libadd"
    zip -q -0 -X "$unsigned" "lib/$abi/libglim-hello.so"
)

aligned="$workdir/aligned.apk"
"$zipalign" -f -p 4 "$unsigned" "$aligned"

keystore="${HOME}/.android/debug.keystore"
if [[ ! -f "$keystore" ]]; then
    mkdir -p "${HOME}/.android"
    keytool -genkeypair -keystore "$keystore" -storepass android -keypass android \
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" >/dev/null
fi

mkdir -p "$(dirname "$out")"
"$apksigner" sign --ks "$keystore" --ks-pass pass:android --key-pass pass:android \
    --ks-key-alias androiddebugkey --out "$out" "$aligned"
"$zipalign" -c -p 4 "$out"
echo "APK $out"
