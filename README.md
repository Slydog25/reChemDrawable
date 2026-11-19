# reChemDrawable

A tap-first, ChemDraw-style 2D chemical structure editor for reMarkable Paper Pro.

The current milestone is a desktop Qt Quick prototype with a testable C++ molecule core. It deliberately avoids handwriting recognition and live drag rendering. Those are not required to produce clean, editable skeletal structures.

## What works in this foundation

- Stable atom and bond IDs, graph editing, hit testing, atom movement, and undo/redo.
- Element labels (C, H, B, N, O, F, Si, P, S, Cl, Se, Br, I, Li, Na, K, Mg), formal charges, isotopes, single/double/triple/aromatic bonds, and wedge/dash/wavy bond styles.
- Formula (isotope labels shown as e.g. `[13C]`), molecular weight, and exact-mass summaries for the drawn structure.
- MDL Molfile V2000 read/write with charge, isotope, aromatic, and wedge/dash/wavy preservation, plus SVG and SMILES export.
- On the tablet the pen draws and selects; a finger only works the menus and dialogs around the canvas, so a resting hand never draws. While the pen is down, finger touches are ignored.
- Finger gestures on the tablet: a quick two-finger tap undoes, a three-finger tap redoes, and two fingers dragged or pinched on the canvas pan and zoom it.
- The Marker's eraser end erases whatever it passes over (atoms, bonds, arrows, labels); one stroke is one undo step. AppLoad reports eraser strokes as ordinary pen input, so the app checks the pen's own input device (`BTN_TOOL_RUBBER`) when a stroke starts. On desktop, right-drag erases, and the Erase tool erases by tapping or dragging.
- Tapping an atom adds a bond at standard skeletal-formula angles: chains zig-zag at 120°, a chain atom branches at 120° to both bonds, ring substituents point straight out, and bonds next to a triple bond continue in a line. Substituent stamps (COOH, NO2, Ph, ...) follow the same direction, and the snap grid's points sit on those angles.
- A monochrome QML editor with tap-to-place/tap-to-connect editing, drag-to-move, pan, zoom buttons and Fit (plus wheel and pinch zoom on desktop), and common fragment stamps (OH, NH2, Me, COOH, CN, NO2, tBu, and Ph).
- **Clean** straightens bond lengths and angles: 120° in chains, 180° beside triple bonds, and regular-polygon angles in rings.
- Lasso selection (draw a loop around atoms), multi-atom move/rotate/mirror/align/delete, copy/paste and one-tap **Duplicate** of selected fragments; desktop builds also accept the standard undo/redo and copy/paste keyboard shortcuts.
- Textbook atom labels: implicit hydrogens on heteroatoms ("OH", "NH₂", written "HO", "H₂N" when the bond comes from the right), isotopes as superscripts, charges as superscripts or circled (⊕/⊖ drawn, toggle "(+)"), lone pairs (LP) and radical dots (•), and a red ring on atoms with impossible valence.
- Group abbreviations (Me, Et, iPr, tBu, Ph, Bn, Ac, Bz, Boc, Cbz, Fmoc, Ts, Ms, Tf, TMS, TBS, TIPS, OMe, OAc, OTs, OTf, OTBS, NHBoc, CF₃, CHO, CO₂H, CO₂Me, CO₂Et, CN, NO₂, ...) from **Abbrev…**: drawn as one label, mirrored when needed, and counted fully in formula, mass, SMILES and Molfile (as aliases).
- **Chain** tool: drag from an atom or empty space and a zig-zag chain follows the pen with a live carbon count.
- Ring templates: 3- to 8-membered rings, benzene, a chair cyclohexane, pyridine, pyrimidine, furan, thiophene, pyrrole, imidazole and indole. Rings fuse onto a tapped bond on the uncrowded side; heterocycles attach to a tapped atom as substituents.
- The Style tool cycles wedge/dash/wavy on single bonds, and centred/left/right placement of the second line on double bonds.
- Optional R/S and E/Z labels (toolbar **R/S**) from wedges and double-bond geometry (CIP rules 1a/1b).
- **Bold lines** toggle for thicker bonds and larger labels on e-ink.
- One undo/redo timeline covering both structure edits and reaction arrows/labels; **Clear** undoes in a single step.
- Reaction-scheme arrows: forward, equilibrium ⇌, retrosynthesis ⇒, resonance ↔, dashed, no-reaction, and curved electron-pushing arrows (full head and fishhook) that bend the way the pen moved. Arrows are drawn by dragging (or tap start, tap end).
- Condition/reagent labels snap above or below an arrow; compound numbers ("1", "2a") are bold; "+" separates reactants. Common labels (Δ, hν, rt, reagents, solvents) are one tap away, since the tablet has no keyboard. Arrows and labels are saved with the molecule in a portable `.rcd` JSON document and included in SVG export.
- Mechanism notation (Mechanism… popup, then tap or drag on the canvas): δ+ / δ− labels, a double dagger ‡ (drawn as strokes, not a font glyph, so it cannot go missing on the tablet), square brackets around an intermediate or transition state (optionally with ‡ at the top right), p-orbital sketches (two lobes along the drag direction, optionally one shaded) and dashed "partial" bonds for forming/breaking bonds. A partial bond is connectivity only: it uses no valence, adds no implicit hydrogens and is left out of SMILES and the formula; a `.mol` file holds it as a plain single bond and the `.rcd` file keeps the style in `partialBonds`. The Style tool also reaches it at the end of its cycle.
- Check compares element counts (implicit H and abbreviations included) and total charge of the molecules before and after each straight reaction arrow and reports "Balanced" or the differences. Tidy lays the whole scheme out left to right on one baseline with ~3-bond-length arrows, moving condition labels with their arrows, as a single undo step; curved arrows and mechanism marks are not moved.
- SMILES import with automatic 2D layout (rings, fused and spiro systems, zig-zag chains): **Import SMILES** next to the text field on desktop, or open a `.smi` file from the drawings folder on the tablet.

## Desktop build

Install Qt 6 with the Core, Gui, Qml, Quick, Test, and QuickTest components, then run:

```sh
./build-pc.sh
```

The executable is placed in `build-pc/`. The script also runs the core tests and the QML canvas tests. To build and test without installing Qt on the host, use the Docker image described in [AGENTS.md](AGENTS.md); GitHub Actions runs the same steps on every push.

## Paper Pro build

From macOS or Linux, with Docker: download the ferrari SDK installer that matches the tablet's software version into `packaging/docker/sdk/`, then run `./build-rmpp-docker.sh`. Details are in [packaging/docker/README.md](packaging/docker/README.md).

Inside an already-sourced SDK shell, `./build-rmpp.sh` does the same build directly. It checks that the compiler targets aarch64 Linux and passes the SDK's CMake toolchain file to CMake.

Before deploying, copy the manifest format from the installed AppLoad release into `packaging/appload/external.manifest.json`; AppLoad metadata is community-tool-version-specific and must be validated on the actual device.

## Coordinates and chemistry conventions

Canvas coordinates use a 48-pixel standard bond length. Molfile export converts these into 1.0-unit 2D coordinates and flips the Y axis, so structures retain their proportions in desktop chemistry tools. Atoms are always explicit in the graph and in Molfiles; carbon labels are merely hidden in the renderer.

Valence checking is intentionally lightweight. It prevents common accidental over-bonding and accounts for formal charge (N⁺ and O⁺ may carry one more bond, C⁺/C⁻ and anions one fewer), but does not attempt aromaticity perception, charge balancing, canonical SMILES, or 3D chemistry. SMILES export follows drawing order (it is not canonical) and does not encode wedge/dash stereo.

## Saving and reopening

Drawings live in `reChemDrawable/` inside the platform's Documents folder.

- **Save** writes the drawing as an `.rcd` file (the structure plus arrows and labels). A new drawing gets a timestamped name such as `drawing-20261118-1530.rcd`; later saves update that same file. The title bar shows the name, with `•` while there are unsaved changes.
- **Open** lists the `.rcd` and `.mol` files in that folder, newest first. Opening a `.mol` starts a drawing without annotations, and saving it creates a separate `.rcd`, so the original Molfile is never overwritten.
- Once a drawing has a file, it is saved automatically a few seconds after each change. **Open** shows a preview, the date and the formula of each drawing, and can delete drawings (with a confirmation).
- **New**, **Open**, **Exit**, and closing the window ask before discarding unsaved changes. Closing the app from AppLoad's X instead saves unsaved changes as `recovered-<time>.rcd` (it cannot show a dialog there), and those files appear in Open.
- **.mol**, **SVG**, and **SMILES** write exports named after the drawing into the `exports/` subfolder.

Files saved by older builds (`structure.mol`, `reaction-scheme.rcd`) appear in the Open list.
