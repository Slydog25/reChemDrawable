# Cross-build container

## Desktop Qt build

`Dockerfile.desktop` is the everyday development environment. It installs Qt 6,
CMake, and the QML modules this app uses, then builds and tests the project
without putting Qt on the host operating system:

```sh
docker build -t rechemdraw-qt -f packaging/docker/Dockerfile.desktop .
docker run --rm -v "$PWD:/workspace" -w /workspace rechemdraw-qt ./build-pc.sh
```

VS Code can use the same environment: install Microsoft's **Dev Containers**
extension, then run **Dev Containers: Reopen in Container**. The checked-in
`.devcontainer/devcontainer.json` selects `Dockerfile.desktop` and recommends
CMake Tools and clangd.

## Paper Pro cross-build

`Dockerfile.rmpp` installs reMarkable's official Paper Pro (codename **ferrari**)
SDK into an image, and `./build-rmpp-docker.sh` builds the tablet binary in it.
This works on macOS as well as Linux; no separate Linux machine is needed.

1. Find your tablet's software version under **Settings > General > About**.
2. From <https://developer.remarkable.com/links>, download the ferrari
   toolchain for that exact version into `packaging/docker/sdk/` (ignored by
   git). On an Apple Silicon Mac take the `aarch64` installer, published from
   software 3.27.0.97 on, so the build runs without emulation; otherwise take
   the `x86_64` one. They are roughly 400 MB.
3. Run:

   ```sh
   ./build-rmpp-docker.sh
   ```

   It picks the newest ferrari installer in `packaging/docker/sdk/` (or the
   one passed as an argument), builds one image per SDK version, and writes
   `build-rmpp/reChemDrawable`.
4. Deploy over USB with `packaging/appload/deploy-appload.sh` (see
   `packaging/appload/README.md`).

The SDK is not downloaded automatically: it must match the tablet's software
version, and it is distributed by reMarkable.
