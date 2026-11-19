#!/bin/sh

set -u

app_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$app_dir"

# AppLoad starts apps without HOME. Qt then treats / as the home folder, so
# Documents/reChemDrawable lands on the read-only root and every save (and the
# recovery copy on close) silently fails.
if [ -z "${HOME:-}" ] || [ "$HOME" = / ]; then
    HOME=$(awk -F: -v uid="$(id -u)" '$3 == uid { print $6; exit }' /etc/passwd)
    export HOME="${HOME:-/home/root}"
fi

if [ -n "${QTFB_KEY:-}" ]; then
    export QT_QPA_PLATFORM=offscreen
    export QT_QUICK_BACKEND=software
fi

# exec replaces this shell with the app, so when AppLoad stops the process it
# started, it stops the app itself. Running the app as a child left it alive in
# the background after the window was closed.
exec ./reChemDrawable >"$app_dir/reChemDrawable.log" 2>&1
