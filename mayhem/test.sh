#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Runs a slice of tensorstore's own gtest suite (built by build.sh with the project's normal
# flags into mayhem/out/tests): zarr metadata + dtype parsing and the
# neuroglancer compressed-segmentation codec — all code the fuzz targets reach. Counts come from
# gtest's own summary lines; a binary that prints no "[  PASSED  ]" line counts as failed.
TESTS=(metadata_test dtype_test neuroglancer_compressed_segmentation_test)
passed=0; failed=0; skipped=0
for t in "${TESTS[@]}"; do
  bin="$SRC/mayhem/out/tests/$t"
  if [ ! -x "$bin" ]; then echo "missing $bin (build.sh should have built it)" >&2; failed=$((failed+1)); continue; fi
  out="$(cd "$SRC" && "$bin" --gtest_color=no 2>&1)"; rc=$?
  echo "$out" | tail -n 5
  p=$(echo "$out" | sed -n 's/^\[  PASSED  \] \([0-9]*\) test.*/\1/p' | tail -1)
  f=$(echo "$out" | sed -n 's/^\[  FAILED  \] \([0-9]*\) test.*/\1/p' | tail -1)
  s=$(echo "$out" | sed -n 's/^\[  SKIPPED \] \([0-9]*\) test.*/\1/p' | tail -1)
  if [ -z "$p" ]; then failed=$((failed+1)); echo "$t: no gtest summary (rc=$rc)" >&2; continue; fi
  passed=$((passed+p)); failed=$((failed+${f:-0})); skipped=$((skipped+${s:-0}))
  if [ "$rc" -ne 0 ] && [ -z "$f" ]; then failed=$((failed+1)); fi
done
emit_ctrf "gtest" "$passed" "$failed" "$skipped"
