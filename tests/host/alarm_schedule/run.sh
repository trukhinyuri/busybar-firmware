#!/bin/sh
#
# Build and run the alarm scheduling tests on the host.
#
# alarm_schedule.c includes "alarm_i.h" with quotes, which a C compiler resolves
# from the including file's own directory first. Building from a copy in a
# scratch directory is what lets the stub in fake/ win instead, so the pure
# scheduling rules can be tested without furi or any firmware service.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
source_file="$here/../../../applications/services/alarm/alarm_schedule.c"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cp "$source_file" "$work/alarm_schedule.c"

cc -std=gnu2x -Wall -Wextra -Werror \
    -I "$here/fake" \
    -o "$work/alarm_tests" \
    "$here/test_alarm_schedule.c" \
    "$work/alarm_schedule.c"

"$work/alarm_tests"
