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

remote() { ssh -p "$device_port" "root@$device_host" "$@"; }

# Upload into a staging folder, then rename each file into place. A rename
# replaces even a running binary (the open copy keeps its old file), and each
# file switches over at once, so the app can stay open during a deploy and the
# next launch always sees a complete new version.
staging="$destination/.incoming"
remote "rm -rf '$staging' && mkdir -p '$staging/qml' '$destination/qml'"
scp -q -P "$device_port" "$binary" \
    "packaging/appload/external.manifest.json" \
    "packaging/appload/reChemDrawable.sh" \
    "root@$device_host:$staging/"
scp -q -P "$device_port" src/qml/*.qml "root@$device_host:$staging/qml/"
remote "set -e
chmod 755 '$staging/reChemDrawable' '$staging/reChemDrawable.sh'
for file in '$staging'/qml/*.qml; do mv -f \"\$file\" '$destination/qml/'; done
for file in external.manifest.json reChemDrawable.sh reChemDrawable; do mv -f '$staging'/\$file '$destination/'; done
rm -rf '$staging'"

echo "Deployed to $device_host:$destination"
if remote "pidof reChemDrawable >/dev/null"; then
    echo "Note: reChemDrawable is still running on the tablet (AppLoad can keep a closed app in the" >&2
    echo "background). It keeps the old version until it is fully quit and opened again." >&2
fi
