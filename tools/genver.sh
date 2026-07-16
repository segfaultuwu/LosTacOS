#!/usr/bin/env bash

set -e

VERSION_FILE="version.txt"
OUTPUT_FILE="build/generated/LTOS_gen/version.h"

if [ ! -f "$VERSION_FILE" ]; then
  echo "error: $VERSION_FILE not found"
  exit 1
fi

VERSION=$(cat "$VERSION_FILE" | tr -d '[:space:]')

if [ -z "$VERSION" ]; then
  echo "error: empty version"
  exit 1
fi

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

FULL_VERSION="${VERSION}-${COMMIT}"

mkdir -p "$(dirname "$OUTPUT_FILE")"

cat >"$OUTPUT_FILE" <<EOF
#pragma once

#define LTOS_VERSION "$FULL_VERSION"
#define LTOS_VERSION_BASE "$VERSION"
#define LTOS_BUILD_COMMIT "$COMMIT"
EOF

echo "Generated $OUTPUT_FILE: $FULL_VERSION"
