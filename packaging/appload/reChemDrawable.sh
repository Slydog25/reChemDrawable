#!/bin/sh

set -u

app_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$app_dir"

if [ -n "${QTFB_KEY:-}" ]; then
    export QT_QPA_PLATFORM=offscreen
    export QT_QUICK_BACKEND=software
fi

./reChemDrawable >"$app_dir/reChemDrawable.log" 2>&1
exit_status=$?
printf 'reChemDrawable exit status: %s\n' "$exit_status" >>"$app_dir/reChemDrawable.log"
exit "$exit_status"
