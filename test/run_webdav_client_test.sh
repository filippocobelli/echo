#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/webdav_client"
BINARY="$BUILD_DIR/WebDavClientTest"

mkdir -p "$BUILD_DIR/obj"

SOURCES=(
  "$ROOT_DIR/test/webdav_client/WebDavClientTest.cpp"
  "$ROOT_DIR/src/network/WebDavClient.cpp"
  "$ROOT_DIR/src/util/UrlUtils.cpp"
  "$ROOT_DIR/lib/WebDavParser/WebDavParser.cpp"
  "$ROOT_DIR/lib/expat/xmlparse.c"
  "$ROOT_DIR/lib/expat/xmlrole.c"
  "$ROOT_DIR/lib/expat/xmltok.c"
)

# Builds the SIMULATOR transport path, with a stubbed sim_http_fetch standing in
# for curl so the client is exercised offline.
COMMON_FLAGS=(
  -O2
  -DSIMULATOR
  -I"$ROOT_DIR"
  -I"$ROOT_DIR/src"
  -I"$ROOT_DIR/test/webdav_client/stubs"
  -I"$ROOT_DIR/lib/WebDavParser"
  -I"$ROOT_DIR/lib/expat"
  -I"$ROOT_DIR/lib/XmlParserUtils"
  -I"$ROOT_DIR/lib/AppVersion"
  -DXML_GE=0
  -DXML_CONTEXT_BYTES=1024
  -DHAVE_EXPAT_CONFIG_H
)

OBJECTS=()
for src in "${SOURCES[@]}"; do
  obj="$BUILD_DIR/obj/$(basename "${src%.*}").o"
  if [[ "$src" == *.c ]]; then
    cc "${COMMON_FLAGS[@]}" -std=c11 -w -c "$src" -o "$obj"
  else
    c++ "${COMMON_FLAGS[@]}" -std=c++20 -Wall -Wextra -c "$src" -o "$obj"
  fi
  OBJECTS+=("$obj")
done

c++ "${OBJECTS[@]}" -o "$BINARY"

"$BINARY" "$@"
