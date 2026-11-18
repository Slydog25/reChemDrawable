# reChemDrawable

A tap-first, ChemDraw-style 2D chemical structure editor for reMarkable Paper Pro.

The current milestone is a desktop Qt Quick prototype with a testable C++ molecule core. It deliberately avoids handwriting recognition and live drag rendering. Those are not required to produce clean, editable skeletal structures.

## What works in this foundation

- Stable atom and bond IDs, graph editing, hit testing, atom movement, and undo/redo.
- Element labels, formal charges, isotopes, single/double/triple/aromatic bonds, and wedge/dash/wavy bond styles.
- Formula, molecular weight, and exact-mass summaries for the drawn structure.
- MDL Molfile V2000 read/write with charge, isotope, aromatic, and wedge/dash/wavy preservation, plus SVG export.
- A monochrome QML editor with tap-to-place/tap-to-connect editing, drag-to-move, pan, wheel zoom, and common fragment stamps (OH, NH2, Me, COOH, CN, NO2, tBu, and Ph).
- Box selection, multi-atom move/rotate/mirror/align/delete, and copy/paste of complete selected fragments; desktop builds also accept the standard undo/redo and copy/paste keyboard shortcuts.
- One undo/redo timeline covering both structure edits and reaction arrows/labels; **Clear** undoes in a single step.
- Reaction-scheme arrows and condition/reagent text labels, saved together with the molecule in a portable `.rcd` JSON document and included in SVG scheme export.

## Desktop build

Install Qt 6 with the Core, Gui, Qml, Quick, and Test components, then run:

```sh
./build-pc.sh
```

The executable is placed in `build-pc/`. To build and test without installing Qt on the host, use the Docker image described in [AGENTS.md](AGENTS.md).

## Paper Pro build

The device SDK setup is intentionally not hard-coded: source the environment script supplied by the SDK that matches the tablet firmware, then run `./build-rmpp.sh` in that same shell. The script verifies that the selected compiler targets aarch64.

Before deploying, copy the manifest format from the installed AppLoad release into `packaging/appload/external.manifest.json`; AppLoad metadata is community-tool-version-specific and must be validated on the actual device.

## Coordinates and chemistry conventions

Canvas coordinates use a 48-pixel standard bond length. Molfile export converts these into 1.0-unit 2D coordinates and flips the Y axis, so structures retain their proportions in desktop chemistry tools. Atoms are always explicit in the graph and in Molfiles; carbon labels are merely hidden in the renderer.

Valence checking is intentionally lightweight. It prevents common accidental over-bonding and accounts for formal charge (N⁺ and O⁺ may carry one more bond, C⁺/C⁻ and anions one fewer), but does not attempt aromaticity perception, charge balancing, canonical SMILES, or 3D chemistry.

## Saving and reopening

The in-app **Save** and **Load** controls use a single predictable file named
`structure.mol` inside the platform's Documents folder under `reChemDrawable/`.
This is deliberately a v1 choice for the keyboard-free Paper Pro workflow. The
core also exposes path-based load/save methods, so a future file-picker or sync
integration can use arbitrary `.mol` locations without changing the format.

For reaction diagrams, **Save scheme** and **Load scheme** use `reaction-scheme.rcd` in
the same folder. An `.rcd` preserves both the embedded Molfile and the editor's arrows
and text labels; it is the native document format, while `.mol` remains the interchange
format for an individual molecular structure.
