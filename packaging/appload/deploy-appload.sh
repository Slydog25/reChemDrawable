#!/usr/bin/env sh
set -eu

device_host=${1:?Usage: deploy-appload.sh DEVICE_HOST APPLOAD_DIRECTORY [SSH_PORT] [BINARY]}
destination=${2:?Usage: deploy-appload.sh DEVICE_HOST APPLOAD_DIRECTORY [SSH_PORT] [BINARY]}
case "${3:-}" in
    "")
        device_port=22
        binary="build-rmpp/reChemDrawable"
        ;;
    *[!0-9]*)
        device_port=22
        binary="$3"
        ;;
    *)
        device_port="$3"
        binary="${4:-build-rmpp/reChemDrawable}"
        ;;
esac

test -x "$binary" || {
    echo "Build the Paper Pro binary before deployment: $binary" >&2
    exit 1
}

ssh -p "$device_port" "root@$device_host" "mkdir -p '$destination'"
scp -P "$device_port" "$binary" \
    "packaging/appload/external.manifest.json" \
    "packaging/appload/reChemDrawable.sh" \
    "root@$device_host:$destination/"
scp -P "$device_port" -r "src/qml" "root@$device_host:$destination/"
ssh -p "$device_port" "root@$device_host" "chmod 755 '$destination/reChemDrawable' '$destination/reChemDrawable.sh'"
