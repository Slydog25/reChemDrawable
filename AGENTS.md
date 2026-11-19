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
- Tablet input goes through `InputRouter` (`src/platform/`): pen everywhere, finger only outside the canvas (`objectName: "moleculeCanvas"`) or on an open dialog (`dialogOpen` on the window).
- After changing `src/qml/ToolbarIcon.qml`, render every icon and look at the result: `QT_QPA_PLATFORM=offscreen ./build-pc/qml_tests -input tools/iconsheet` writes `build-pc/icon-sheet.png`.
- After UI or drawing changes, render the whole app at the tablet's 1620x2160 and look at it: `QT_QPA_PLATFORM=offscreen ./build-pc/qml_tests -input tools/screens` writes `build-pc/screen-*.png`.
- The core and the testable input helpers build once as the `rechemdraw_core` and `rechemdraw_platform` libraries. Add a C++ test file with one line in CMakeLists.txt: `rechemdraw_add_test(<name> tests/<file>.cpp)`; QML tests are any `tests/qml/tst_*.qml`.
- Edits are undone by restoring whole snapshots. `snapshot()` takes a real copy of the lists, so it is safe to hold an atom/bond pointer across it; keep it that way (`tests/test_undo.cpp`).
- Data the Molfile cannot hold (lone pairs, circled-charge setting, double-bond sides, arrow kinds and curves) lives in the `.rcd` JSON next to the embedded Molfile, keyed by atom/bond index, and must load from older files without those keys.
- Carbon is stored explicitly but drawn without a label. Canvas coordinates use `GeometryEngine::StandardBondLength`; Molfile coordinates are converted at the serialization boundary.
- The Paper Pro build must use the official SDK that matches the device firmware. Do not substitute a generic aarch64 build for device validation.

## Current intentional limits

The editor is pen-first: no handwriting recognition or freehand ink. Formal charges, isotopes, and wedge/dash/wavy bonds are drawn and round-tripped through Molfiles, but the only stereo perception is the optional R/S and E/Z label overlay (`src/core/Stereo.cpp`; CIP rules 1a/1b only, no descriptor when a tie remains). Do not introduce a third-party chemistry library without an explicit decision to do so.
