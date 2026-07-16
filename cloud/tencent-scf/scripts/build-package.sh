#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(dirname -- "$SCRIPT_DIR")
REPO_DIR=$(CDPATH= cd -- "$PACKAGE_DIR/../.." && pwd)
OUTPUT=${1:-"$REPO_DIR/.build/tencent-scf-safe-mode.zip"}

mkdir -p "$(dirname -- "$OUTPUT")"
OUTPUT_DIR=$(CDPATH= cd -- "$(dirname -- "$OUTPUT")" && pwd)
OUTPUT_PATH="$OUTPUT_DIR/$(basename -- "$OUTPUT")"

STAGING=$(mktemp -d)
trap 'rm -rf "$STAGING"' EXIT HUP INT TERM
mkdir -p "$STAGING/package"

cp "$PACKAGE_DIR/package.json" "$PACKAGE_DIR/package-lock.json" "$STAGING/package/"
cp "$PACKAGE_DIR/monitor.js" "$PACKAGE_DIR/serializer.js" "$STAGING/package/"
cp -R "$PACKAGE_DIR/src" "$STAGING/package/src"

(cd "$STAGING/package" && npm ci \
  --omit=dev \
  --ignore-scripts \
  --no-audit \
  --no-fund)

(cd "$STAGING/package" && zip -q -r "$STAGING/tencent-scf.zip" . \
  -x '*/test/*' '*/tests/*' '*/__tests__/*' \
     '*/test.js' '*/tests.js' '*/test-*.js' '*.test.js' \
     '*/test.cjs' '*/tests.cjs' '*/test-*.cjs' '*.test.cjs' \
     '*/test.mjs' '*/tests.mjs' '*/test-*.mjs' '*.test.mjs' \
     '*/test.ts' '*/tests.ts' '*/test-*.ts' '*.test.ts')
mv "$STAGING/tencent-scf.zip" "$OUTPUT_PATH"
echo "$OUTPUT_PATH"
