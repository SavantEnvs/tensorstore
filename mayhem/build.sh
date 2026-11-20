#!/usr/bin/env bash
#
# mayhem/build.sh — build tensorstore's fuzz harnesses (mayhem/harnesses) and a slice of
# its own gtest suite with Bazel.
#
# Bazel and its whole dependency closure live under /opt/toolchains/bazel (fixed,
# $HOME-independent): bin/bazel (pinned release), repocache/ (every downloaded archive)
# and root/ (output_user_root: the extracted external repos + the action cache). The
# Dockerfile's first, online run fills them; a re-run (PATCH tier) is offline and
# incremental, because the output base lives outside the source tree and survives
# `git clean -ffdX`.
#
# Fuzz build (-c fastbuild): $SANITIZER_FLAGS on every C/C++ compile and link (project +
# deps), SanCov (-fsanitize=fuzzer-no-link) only on tensorstore/ and mayhem/ sources.
# Debug info is DWARF 3 everywhere: line tables (file:line in every backtrace frame) for the
# library and deps, full $DEBUG_FLAGS for the harnesses. Full -g on all of tensorstore + its
# deps makes the output base too large for a CI runner's disk.
# Test build (-c opt): the project's normal flags, in its own output dir (k8-opt), so the
# two configurations never invalidate each other.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${BAZEL_HOME:=/opt/toolchains/bazel}"
cd "$SRC"

BAZEL="$BAZEL_HOME/bin/bazel"
[ -x "$BAZEL" ] || { echo "missing $BAZEL" >&2; exit 1; }
STARTUP=(--output_user_root="$BAZEL_HOME/root" --nohome_rc --nosystem_rc)
COMMON=(
  --repository_cache="$BAZEL_HOME/repocache"
  --repo_env=CC="$CC" --repo_env=CXX="$CXX" --repo_env=BAZEL_COMPILER=clang
  --spawn_strategy=local
  --experimental_convenience_symlinks=ignore
  --jobs="$MAYHEM_JOBS"
  --strip=never
)

# LeakSanitizer is off in every fuzz and -standalone binary: mayhem/harnesses/fuzz.bzl links
# :lsan_off (mayhem/harnesses/lsan_off.cc, __lsan_is_turned_off) into both.
# The run-once driver for the -standalone reproducers (gitignored, recreated every run).
cp -f "$STANDALONE_FUZZ_MAIN" mayhem/harnesses/standalone_main.c

FUZZERS=(zarr_fuzzer zarr3_fuzzer n5_fuzzer neuroglancer_precomputed_fuzzer
         png_fuzzer tiff_fuzzer bmp_fuzzer webp_fuzzer zip_kvstore_fuzzer spec_fuzzer
         auto_fuzzer serialization_fuzzer ocdbt_format_fuzzer)
TESTS=(//tensorstore/driver/zarr:metadata_test
       //tensorstore/driver/zarr:dtype_test
       //tensorstore/internal/compression:neuroglancer_compressed_segmentation_test)

FUZZ_OPTS=(-c fastbuild)
for f in $SANITIZER_FLAGS; do FUZZ_OPTS+=(--copt="$f" --linkopt="$f"); done
FUZZ_OPTS+=(--copt=-O1 --copt=-gline-tables-only --copt=-gdwarf-3)
FUZZ_OPTS+=("--per_file_copt=^tensorstore/.*,^mayhem/.*@-fsanitize=fuzzer-no-link")
pf=""; for f in $DEBUG_FLAGS; do pf="${pf:+$pf,}$f"; done
FUZZ_OPTS+=("--per_file_copt=^mayhem/.*@$pf")
# UBSan relaxation (only this one check): tensorstore's MultiVectorStorage copy does
# memcpy(nullptr, nullptr, 0) for empty vectors (internal/container/multi_vector.h), which the
# nonnull-attribute check reports on every input. Everything else in ASan/UBSan stays halting.
[ -n "$SANITIZER_FLAGS" ] && FUZZ_OPTS+=(--copt=-fno-sanitize=nonnull-attribute --linkopt=-fsanitize-link-c++-runtime)

targets=()
for t in "${FUZZERS[@]}"; do targets+=("//mayhem/harnesses:$t" "//mayhem/harnesses:$t-standalone"); done
"$BAZEL" "${STARTUP[@]}" build "${COMMON[@]}" "${FUZZ_OPTS[@]}" "${targets[@]}"
bin="$("$BAZEL" "${STARTUP[@]}" info "${COMMON[@]}" "${FUZZ_OPTS[@]}" bazel-bin)"

mkdir -p mayhem/out
for t in "${FUZZERS[@]}"; do
  install -m 0755 "$bin/mayhem/harnesses/$t" "mayhem/out/$t"
  install -m 0755 "$bin/mayhem/harnesses/$t-standalone" "mayhem/out/$t-standalone"
done

# The project's own tests, normal flags.
"$BAZEL" "${STARTUP[@]}" build "${COMMON[@]}" -c opt --dynamic_mode=off "${TESTS[@]}"
tbin="$("$BAZEL" "${STARTUP[@]}" info "${COMMON[@]}" -c opt --dynamic_mode=off bazel-bin)"
mkdir -p mayhem/out/tests
for t in "${TESTS[@]}"; do
  p="${t#//}"; p="${p/://}"
  install -m 0755 "$tbin/$p" "mayhem/out/tests/$(basename "$p")"
done
"$BAZEL" "${STARTUP[@]}" shutdown || true
echo "built: ${FUZZERS[*]}"
