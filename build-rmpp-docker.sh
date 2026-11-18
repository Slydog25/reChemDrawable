#!/usr/bin/env sh
# Builds the Paper Pro binary with reMarkable's official SDK inside Docker, so
# it works from macOS (including Apple Silicon) as well as Linux.
#
# Usage: ./build-rmpp-docker.sh [path/to/ferrari-toolchain.sh]
#
# Without an argument, the newest ferrari installer in packaging/docker/sdk/ is
# used, preferring one built for this machine's CPU (no emulation). The result
# is build-rmpp/reChemDrawable; deploy it with packaging/appload/deploy-appload.sh.
set -eu

sdk_dir="packaging/docker/sdk"

case "$(uname -m)" in
    arm64|aarch64) native=aarch64; other=x86_64 ;;
    *) native=x86_64; other=aarch64 ;;
esac

newest_installer() {
    # Installer names embed the OS version, e.g.
    # remarkable-production-image-6.0.112-ferrari-public-aarch64-toolchain.sh
    ls "$sdk_dir"/*ferrari*-"$1"-toolchain.sh 2>/dev/null | sort -V | tail -n 1
}

installer=${1:-}
if [ -z "$installer" ]; then
    installer=$(newest_installer "$native")
    [ -n "$installer" ] || installer=$(newest_installer "$other")
fi
if [ -z "$installer" ] || [ ! -f "$installer" ]; then
    cat >&2 <<EOF
No Paper Pro SDK installer found.

Download the ferrari toolchain that matches your tablet's software version
(Settings > General > About) from https://developer.remarkable.com/links
into $sdk_dir/. On an Apple Silicon Mac pick the "aarch64" one
(available from software 3.27.0.97); otherwise pick "x86_64".
EOF
    exit 1
fi

case "$installer" in
    *-aarch64-toolchain.sh) platform=linux/arm64 ;;
    *-x86_64-toolchain.sh) platform=linux/amd64 ;;
    *) echo "Cannot tell the SDK host architecture from: $installer" >&2; exit 1 ;;
esac
if [ "$platform" = linux/amd64 ] && [ "$native" = aarch64 ]; then
    echo "Note: using an x86_64 SDK on an ARM machine; Docker will emulate it (slower)." >&2
fi

name=$(basename "$installer")
# One image per SDK version, so switching firmware does not reinstall the SDK.
image="rechemdraw-rmpp:$(printf '%s' "$name" | sed -e 's/\.sh$//' -e 's/[^A-Za-z0-9_.-]/-/g' | cut -c1-120)"

echo "Using $name ($platform)"
docker build --platform "$platform" -t "$image" \
    --build-arg SDK_INSTALLER="$name" \
    -f packaging/docker/Dockerfile.rmpp "$(dirname "$installer")"

# A desktop build directory configured elsewhere would confuse CMake; this one
# is only ever configured inside the SDK container.
docker run --rm --platform "$platform" -v "$PWD:/workspace" -w /workspace "$image" \
    sh -c '. /opt/rmpp-sdk/environment-setup-* && ./build-rmpp.sh'

echo
file build-rmpp/reChemDrawable 2>/dev/null || ls -l build-rmpp/reChemDrawable
