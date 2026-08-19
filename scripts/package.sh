#!/usr/bin/env bash
set -euo pipefail

TEAM_ID="W37L5728Y6"
APP_CERT="Developer ID Application: Dzmitry Sharko ($TEAM_ID)"
INST_CERT="Developer ID Installer: Dzmitry Sharko ($TEAM_ID)"
BUNDLE_ID="com.SharkoAudio.MaxOx"

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENTITLEMENTS="$PROJECT_DIR/Resources/MaxOx.entitlements"
BUILD_DIR="$PROJECT_DIR/build"
VST3_SRC="$BUILD_DIR/MaxOx_artefacts/Release/VST3/MaxOx.vst3"
PKG_DIR="$BUILD_DIR/pkg"

VERSION=$(python3 -c "import json; print(json.load(open('$PROJECT_DIR/package.json'))['version'])")
PKG_NAME="MaxOx-${VERSION}.pkg"

if [ ! -d "$VST3_SRC" ]; then
  echo ">>> Release build not found. Building..."
  cd "$BUILD_DIR"
  cmake .. && cmake --build . --config Release
fi

echo ">>> Removing old signature..."
codesign --remove-signature "$VST3_SRC" 2>/dev/null || true

echo ">>> Signing VST3 bundle with Developer ID (hardened runtime)..."
codesign --force --deep --strict \
  --options runtime \
  --timestamp \
  --entitlements "$ENTITLEMENTS" \
  --sign "$APP_CERT" \
  "$VST3_SRC"

echo ">>> Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$VST3_SRC"

rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/payload"

INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3_SRC" "$PKG_DIR/payload/MaxOx.vst3"

echo ">>> Building component pkg..."
pkgbuild \
  --root "$PKG_DIR/payload" \
  --identifier "$BUNDLE_ID" \
  --version "$VERSION" \
  --install-location "$INSTALL_DIR" \
  "$PKG_DIR/MaxOx-component.pkg"

echo ">>> Building signed product pkg..."
cat > "$PKG_DIR/distribution.xml" <<DIST
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>MaxOx VST3 Plugin</title>
  <welcome mime-type="text/plain" />
  <options customize="never" require-scripts="false" hostArchitectures="arm64" />
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true" />
  <choices-outline>
    <line choice="default">
      <line choice="com.SharkoAudio.MaxOx.vst3" />
    </line>
  </choices-outline>
  <choice id="default" />
  <choice id="com.SharkoAudio.MaxOx.vst3" visible="false">
    <pkg-ref id="com.SharkoAudio.MaxOx" />
  </choice>
  <pkg-ref id="com.SharkoAudio.MaxOx" version="$VERSION">MaxOx-component.pkg</pkg-ref>
</installer-gui-script>
DIST

productbuild \
  --distribution "$PKG_DIR/distribution.xml" \
  --package-path "$PKG_DIR" \
  --sign "$INST_CERT" \
  --timestamp \
  "$PKG_DIR/$PKG_NAME"

echo ">>> Verifying pkg signature..."
pkgutil --check-signature "$PKG_DIR/$PKG_NAME"

echo ">>> Submitting for notarization..."
if ! xcrun notarytool history --keychain-profile "notarytool-profile" >/dev/null 2>&1; then
  echo ""
  echo "ERROR: Keychain profile 'notarytool-profile' not found."
  echo "Create it with:"
  echo "  xcrun notarytool store-credentials notarytool-profile \\"
  echo "    --apple-id YOUR_APPLE_ID --team-id $TEAM_ID --password APP_SPECIFIC_PASSWORD"
  echo ""
  echo "The unsigned pkg is at: $PKG_DIR/$PKG_NAME"
  exit 1
fi

xcrun notarytool submit "$PKG_DIR/$PKG_NAME" \
  --keychain-profile "notarytool-profile" \
  --wait

echo ">>> Stapling notarization ticket..."
xcrun stapler staple "$PKG_DIR/$PKG_NAME"

rm -f "$PKG_DIR/MaxOx-component.pkg" "$PKG_DIR/distribution.xml"
rm -rf "$PKG_DIR/payload"

FINAL="$BUILD_DIR/$PKG_NAME"
mv "$PKG_DIR/$PKG_NAME" "$FINAL"
rmdir "$PKG_DIR" 2>/dev/null || true

echo ""
echo "=== Done: $FINAL ==="
echo ""
spctl --assess --type install --verbose=2 "$FINAL" 2>&1 || true
