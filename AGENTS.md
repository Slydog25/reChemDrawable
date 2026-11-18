# reChemDrawable contributor guide

## Build and test

Run all desktop checks in Docker; do not assume Qt is installed on the host:

```sh
docker build -q -t rechemdraw-qt -f packaging/docker/Dockerfile.desktop .
docker run --rm -v "$PWD:/workspace" -w /workspace rechemdraw-qt ./build-pc.sh
```

For a nonvisual startup smoke test:

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace rechemdraw-qt sh -lc \
  'QT_QPA_PLATFORM=offscreen timeout 3 ./build-pc/reChemDrawable; test $? -eq 124'
```

## Architecture boundaries

- `src/core/` owns graph state, geometry, undo/redo, and Molfile I/O. Keep it UI-independent and add or update a QtTest case in `tests/` for behavior changes.
- `src/qml/` renders core state and turns interactions into calls on `Molecule`; it must not maintain a second molecule model. UI undo/redo goes through `EditHistory`, which keeps one timeline across `Molecule` and `ReactionLayer`. File handling (current path, unsaved changes, Open list, exports) belongs to `Document`.
- Element data (valences, charge rules, masses) lives only in `src/core/Elements.cpp`; valence checks, hydrogen counts, formulas, and SMILES all read it.
- Canvas interaction changes need a case in `tests/qml/tst_canvas.qml`, which drives `MoleculeCanvas` with simulated taps and drags.
- Carbon is stored explicitly but drawn without a label. Canvas coordinates use `GeometryEngine::StandardBondLength`; Molfile coordinates are converted at the serialization boundary.
- The Paper Pro build must use the official SDK that matches the device firmware. Do not substitute a generic aarch64 build for device validation.

## Current intentional limits

The v1 editor is tap-first: no handwriting recognition, freehand ink, live drag preview, or stereochemistry perception. Formal charges, isotopes, and wedge/dash/wavy bonds are drawn and round-tripped through Molfiles, but stereocenters are not interpreted. Do not introduce a third-party chemistry library without an explicit decision to do so.
