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

Build the base shell with:

```sh
docker build -t rechemdraw-rmpp-build packaging/docker
```

Run it while mounting both this checkout and the **firmware-matched official Paper Pro SDK**. Inside the container, source that SDK's `environment-setup-*` script before invoking `/workspace/build-rmpp.sh`.

The SDK is intentionally not downloaded or baked into the image: its version must match the tablet firmware and its installer is distributed by reMarkable.
