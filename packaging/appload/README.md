# AppLoad packaging

The package uses the manifest shape and `qtfb` setting from the installed AppLoad release on the target Paper Pro. `reChemDrawable.sh` starts the binary from its package directory.

After an official-SDK build, deploy with:

```sh
./packaging/appload/deploy-appload.sh 10.11.99.1 /home/root/xovi/exthome/appload/reChemDrawable
```

When macOS reaches the tablet through a local SSH tunnel on port 2222, use:

```sh
./packaging/appload/deploy-appload.sh 127.0.0.1 /home/root/xovi/exthome/appload/reChemDrawable 2222
```
