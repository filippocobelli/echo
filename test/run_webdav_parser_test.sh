#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/webdav_parser"
BINARY="$BUILD_DIR/WebDavParserTest"

mkdir -p "$BUILD_DIR"

SOURCES=(
  "$ROOT_DIR/test/webdav_parser/WebDavParserTest.cpp"
  "$ROOT_DIR/lib/WebDavParser/WebDavParser.cpp"
  "$ROOT_DIR/lib/expat/xmlparse.c"
  "$ROOT_DIR/lib/expat/xmlrole.c"
  "$ROOT_DIR/lib/expat/xmltok.c"
)

# Stubs stand in for the Arduino Print/Stream/Logging headers the parser pulls in.
COMMON_FLAGS=(
  -O2
  -I"$ROOT_DIR"
  -I"$ROOT_DIR/test/webdav_parser/stubs"
  -I"$ROOT_DIR/lib/expat"
  -I"$ROOT_DIR/lib/XmlParserUtils"
  -DXML_GE=0
  -DXML_CONTEXT_BYTES=1024
  -DHAVE_EXPAT_CONFIG_H
)

mkdir -p "$BUILD_DIR/obj"
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
