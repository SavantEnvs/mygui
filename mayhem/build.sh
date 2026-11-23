#!/usr/bin/env bash
# mayhem/build.sh — build MyGUI (sanitized) + the fuzz harnesses, and the upstream unit tests (normal
# flags) that mayhem/test.sh runs. Re-runnable and offline: pugixml/utfcpp (FetchContent upstream)
# come from /opt/toolchains/deps, FreeType from apt.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

PUGIXML_SRC=/opt/toolchains/deps/pugixml-1.16/src
UTFCPP_SRC=/opt/toolchains/deps/utfcpp-4.1.1/src
cd "$SRC"

common_cmake=(
  -G Ninja
  -DBUILD_SHARED_LIBS=OFF
  -DMYGUI_RENDERSYSTEM=1
  -DMYGUI_USE_FREETYPE=ON
  -DMYGUI_MSDF_FONTS=OFF
  -DMYGUI_BUILD_DEMOS=OFF
  -DMYGUI_BUILD_ADVANCED_DEMOS=OFF
  -DMYGUI_BUILD_TOOLS=OFF
  -DMYGUI_BUILD_DOCS=OFF
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DFETCHCONTENT_SOURCE_DIR_PUGIXML="$PUGIXML_SRC"
  -DFETCHCONTENT_SOURCE_DIR_UTFCPP="$UTFCPP_SRC"
)

# 1) Sanitized engine + DummyPlatform. Build type "Fuzz" adds no flags of its own (keeps asserts,
#    and only our -gdwarf-3 debug info).
cmake -S . -B build-fuzz "${common_cmake[@]}" \
  -DCMAKE_BUILD_TYPE=Fuzz \
  -DMYGUI_BUILD_UNITTESTS=OFF \
  -DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target MyGUIEngine MyGUI.DummyPlatform

# 2) Harnesses: embed the stock MyGUI_Media XML for the layout/textview targets, then link each
#    harness twice (libFuzzer + standalone reproducer).
gen=build-fuzz/mayhem-gen
mkdir -p "$gen"
{
  echo "#pragma once"
  echo "#include <cstddef>"
  echo "namespace fuzz_media {"
  i=0; names=()
  for f in Media/MyGUI_Media/*.xml; do
    echo "static const unsigned char kData$i[] = {"
    od -An -v -tx1 "$f" | sed -e 's/ \([0-9a-f][0-9a-f]\)/0x\1,/g'
    echo "};"
    names+=("{\"$(basename "$f")\", kData$i, sizeof(kData$i)}")
    i=$((i+1))
  done
  echo "struct File { const char* name; const unsigned char* data; size_t size; };"
  echo "static const File kFiles[] = {"
  for n in "${names[@]}"; do echo "  $n,"; done
  echo "};"
  echo "}"
} > "$gen/fuzz_media.h.tmp"
mv "$gen/fuzz_media.h.tmp" "$gen/fuzz_media.h"

includes=(-I"$SRC/MyGUIEngine/include" -I"$SRC/build-fuzz/MyGUIEngine/include" -I"$SRC/Platforms/DummyPlatform/include" -I"$SRC/mayhem/fuzz" -I"$gen")
libs=("$SRC/build-fuzz/lib/libMyGUI.DummyPlatform.a" "$SRC/build-fuzz/lib/libMyGUIEngineStatic.a"
      "$(find build-fuzz -name 'libpugixml*.a' | head -1)" -lfreetype -lz)
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build-fuzz/lsan_off.o
for t in fuzz_resources fuzz_layout fuzz_editbox fuzz_truetype_font fuzz_textview fuzz_language fuzz_textiterator; do
  $CXX -std=c++17 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link "${includes[@]}" \
    -c "mayhem/fuzz/$t.cpp" -o "build-fuzz/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build-fuzz/$t.o" build-fuzz/lsan_off.o "${libs[@]}" -o "/mayhem/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS build-fuzz/standalone_main.o "build-fuzz/$t.o" build-fuzz/lsan_off.o "${libs[@]}" \
    -o "/mayhem/$t-standalone"
done

# 3) Upstream unit tests with normal flags (run by mayhem/test.sh).
cmake -S . -B build-tests "${common_cmake[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMYGUI_BUILD_UNITTESTS=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS"
