#!/usr/bin/env bash
# aris-ui — fetch Google Fonts or TTF/OTF files into assets/fonts/
# Usage:
#   ./tools/fetch_font.sh Inter
#   ./tools/fetch_font.sh Roboto
#   ./tools/fetch_font.sh "JetBrains Mono"
#   ./tools/fetch_font.sh <URL> [output_name.ttf]

set -euo pipefail

DEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/assets/fonts"
mkdir -p "$DEST_DIR"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <FontName | URL> [output_name.ttf]"
    echo "Examples:"
    echo "  $0 Inter"
    echo "  $0 Roboto"
    echo "  $0 \"JetBrains Mono\""
    echo "  $0 https://github.com/google/fonts/raw/main/ofl/spacegrotesk/SpaceGrotesk%5Bwght%5D.ttf SpaceGrotesk.ttf"
    exit 1
fi

TARGET="$1"

if [[ "$TARGET" =~ ^https?:// ]]; then
    OUT_NAME="${2:-$(basename "${TARGET%%\?*}")}"
    OUT_PATH="$DEST_DIR/$OUT_NAME"
    echo "Downloading from URL to $OUT_PATH..."
    curl -fL "$TARGET" -o "$OUT_PATH"
    echo "Saved: $OUT_PATH"
    exit 0
fi

# Canonical Google Fonts repository slugs
case "$(echo "$TARGET" | tr '[:upper:]' '[:lower:]' | tr -d ' ')" in
    "inter")
        URL="https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz%2Cwght%5D.ttf"
        OUT_NAME="Inter.ttf"
        ;;
    "roboto")
        URL="https://github.com/google/fonts/raw/main/ofl/roboto/Roboto%5Bwdth%2Cwght%5D.ttf"
        OUT_NAME="Roboto.ttf"
        ;;
    "jetbrainsmono")
        URL="https://github.com/google/fonts/raw/main/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf"
        OUT_NAME="JetBrainsMono.ttf"
        ;;
    "poppins")
        URL="https://github.com/google/fonts/raw/main/ofl/poppins/Poppins-Regular.ttf"
        OUT_NAME="Poppins.ttf"
        ;;
    "outfit")
        URL="https://github.com/google/fonts/raw/main/ofl/outfit/Outfit%5Bwght%5D.ttf"
        OUT_NAME="Outfit.ttf"
        ;;
    "spacegrotesk")
        URL="https://github.com/google/fonts/raw/main/ofl/spacegrotesk/SpaceGrotesk%5Bwght%5D.ttf"
        OUT_NAME="SpaceGrotesk.ttf"
        ;;
    *)
        SLUG=$(echo "$TARGET" | tr '[:upper:]' '[:lower:]' | tr -d ' ')
        NAME=$(echo "$TARGET" | tr -d ' ')
        URL="https://github.com/google/fonts/raw/main/ofl/${SLUG}/${NAME}%5Bwght%5D.ttf"
        OUT_NAME="${NAME}.ttf"
        ;;
esac

OUT_PATH="$DEST_DIR/$OUT_NAME"
echo "Fetching Google Font '$TARGET'..."
if curl -fL "$URL" -o "$OUT_PATH" 2>/dev/null; then
    echo "Successfully downloaded $TARGET to $OUT_PATH"
else
    # Fallback to direct TTF search on google/fonts repo
    echo "Direct variable font URL not found; trying alternative download path..."
    FALLBACK_URL="https://raw.githubusercontent.com/google/fonts/main/ofl/${SLUG}/${NAME}-Regular.ttf"
    curl -fL "$FALLBACK_URL" -o "$OUT_PATH"
    echo "Successfully downloaded $TARGET to $OUT_PATH"
fi
