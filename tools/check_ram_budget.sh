#!/usr/bin/env bash
# See COPYING.txt for license details.
#
# check_ram_budget.sh — Early-warning check for the STM32H573 RAM budget.
#
# The linker's `-Wl,--print-memory-usage` flag (cmake/gcc-arm-none-eabi.cmake)
# already prints a "Memory region ... Used Size ... %age Used" summary on
# every build, and a HARD link failure ("region `RAM' overflowed by N bytes")
# is unavoidable once usage exceeds 100%.  That hard failure is the *last*
# possible moment to notice a RAM problem — by then a PR has to be reworked
# from scratch to claw back headroom (see PR #737 / CI job 100054096523,
# which overflowed by 2344 bytes and required a dedicated RAM-trimming pass).
#
# This script re-parses that same linker summary from a captured build log
# and raises a *non-blocking* GitHub Actions warning annotation whenever RAM
# usage is above a configurable danger threshold — so a PR that is quietly
# eating into the last few KB of headroom gets flagged for review the moment
# it lands, not only once some future PR finally tips it over triggering a
# build failure.
#
# Usage:
#   check_ram_budget.sh <build-log-file> [warn-threshold-percent]
#
# The build log must contain the linker's "Memory region" summary block
# (i.e. the build step must NOT swallow stdout/stderr — tee it to a file).
#
# Exit status is always 0 (this is an advisory check, not a gate) unless the
# summary cannot be found at all, which usually means the build itself
# failed for an unrelated reason (that failure will already have been caught
# by the preceding build step).

set -euo pipefail

LOG_FILE="${1:?usage: check_ram_budget.sh <build-log-file> [warn-threshold-percent]}"
WARN_THRESHOLD="${2:-98}"

if [ ! -f "$LOG_FILE" ]; then
    echo "check_ram_budget.sh: log file not found: $LOG_FILE" >&2
    exit 1
fi

# Match a line like:
#              RAM:      655216 B       640 KB     99.98%
RAM_LINE=$(grep -E '^[[:space:]]*RAM:[[:space:]]+[0-9]+ B' "$LOG_FILE" | tail -1 || true)

if [ -z "$RAM_LINE" ]; then
    echo "check_ram_budget.sh: no linker 'RAM:' memory-usage line found in $LOG_FILE" >&2
    echo "check_ram_budget.sh: skipping RAM budget check (nothing to parse)" >&2
    exit 0
fi

USED_BYTES=$(echo "$RAM_LINE" | grep -Eo '[0-9]+ B' | head -1 | grep -Eo '[0-9]+')
PCT=$(echo "$RAM_LINE" | grep -Eo '[0-9]+\.[0-9]+%' | head -1 | tr -d '%')
REGION_SIZE=655360  # 640 KiB, STM32H573VITX RAM region (STM32H573VITX_FLASH.ld)
FREE_BYTES=$((REGION_SIZE - USED_BYTES))

SUMMARY="RAM usage: ${USED_BYTES} / ${REGION_SIZE} bytes (${PCT}%) — ${FREE_BYTES} bytes free"
echo "$SUMMARY"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "### RAM budget"
        echo
        echo "$SUMMARY"
    } >> "$GITHUB_STEP_SUMMARY"
fi

# Compare percentage against threshold using awk (avoids a bc dependency).
IS_OVER=$(awk -v pct="$PCT" -v thr="$WARN_THRESHOLD" 'BEGIN { print (pct >= thr) ? "1" : "0" }')

if [ "$IS_OVER" = "1" ]; then
    echo "::warning::RAM usage is ${PCT}% (only ${FREE_BYTES} bytes free out of ${REGION_SIZE}) — this firmware is at high risk of a future RAM-overflow build failure. Before adding any new static/global buffers, read .github/skills/memory-heap/SKILL.md (Static RAM Budget section) and prefer stack-local buffers, a shared scratch union, or heap allocation over new permanent .bss/.data statics."
fi

exit 0
