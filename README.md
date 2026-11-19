# reChemDrawable

A pen-first, ChemDraw-style editor for drawing organic structures and reaction schemes on the reMarkable Paper Pro. Draw zig-zag chains, rings and heterocycles, mechanism arrows and conditions with the Marker; erase with its eraser end; save, open, and export to .mol, SMILES, SVG, PNG, PDF and ChemDraw CDXML.

> **Status: experimental.** The core is covered by automated tests on desktop (run in CI on every push), and the app runs on a Paper Pro with software 3.29.1.0. Several of the newest features (partial e-ink updates, the on-screen keyboard, landscape, Send to library) have not been tried on a tablet yet; see [E-ink display, keyboard and landscape](#e-ink-display-keyboard-and-landscape-appload).

## Install on a Paper Pro

You need:
- A reMarkable Paper Pro with **developer mode** enabled (Settings > General > Software > Advanced). reMarkable explains what developer mode means for your device and warranty when you turn it on.
- [XOVI](https://github.com/asivery/xovi) and [AppLoad](https://github.com/asivery/rm-appload) installed on the tablet. reChemDrawable runs as an AppLoad app.
- A computer with Docker to build the app (macOS or Linux), and a USB-C cable.

Steps:
1. **Build** the tablet binary with the official SDK for your tablet's software version: download the "ferrari" toolchain installer from <https://developer.remarkable.com/links> into `packaging/docker/sdk/`, then run `./build-rmpp-docker.sh`. Details: [packaging/docker/README.md](packaging/docker/README.md).
2. **Connect** the tablet by USB and set up SSH key login once: `ssh-copy-id root@10.11.99.1` (the root password is listed in the tablet's settings under Copyrights and licenses once developer mode is on).
3. **Deploy**: `./packaging/appload/deploy-appload.sh 10.11.99.1 /home/root/xovi/exthome/appload/reChemDrawable`. This works even while the app is open; the new version starts the next time you open it.
4. **Open** reChemDrawable from AppLoad. Leave with the **Exit** button (top right), which offers to save first.

Drawings are saved in `Documents/reChemDrawable` on the tablet (`/home/root/Documents/reChemDrawable`), with exports in its `exports/` folder.

## Using it

- **Pen** draws and selects; **fingers** work the menus and dialogs only, so a resting hand never draws. Two-finger tap = Undo, three-finger tap = Redo, two fingers on the canvas pan and pinch-zoom.
- Tap empty canvas to place a carbon, tap an atom to extend it at the right angle, drag from an atom to place a bond where you want it.
- Select an atom (Select tool) to change its element, charge, lone pairs or radical in the palette, or to attach a group (**Abbrev…**).
- The **eraser end** of the Marker erases what it passes over; one stroke undoes in one step.

## Features

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
- **Templates…**: Newman, Fischer and Haworth projections, the 20 L-amino acids (with wedges, so R/S shows S, Cys R), the five nucleobases and common reagents (DMAP, DCC, NBS, TsCl, Boc₂O). Select atoms and **Save selection as template** to keep your own fragments under **Mine** (stored in `templates.json` next to your drawings).
- **Properties** panel for the whole drawing or the selection: formula, MW, exact mass, degrees of unsaturation, H-bond donors/acceptors and rule-of-five, TPSA (Ertl), elemental analysis, [M+H]⁺/[M+Na]⁺/[M−H]⁻ and the isotope pattern as a bar chart.
- Stereochemistry through SMILES: `@`/`@@` become wedges and `/` `\` set cis/trans on import, and SMILES export writes them back. The **Invert** tool flips a tapped stereocentre (wedge ↔ dash) or a double bond (E ↔ Z).
- **Export…**: .mol, SVG, SMILES, PNG (300 dpi), PDF (vector), ChemDraw CDXML, and **Send to reMarkable library**, which puts a PDF of the drawing into the tablet's My files (it appears after the tablet restarts).
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

The AppLoad manifest in `packaging/appload/external.manifest.json` was checked against the AppLoad release installed on a tablet with software 3.29.1.0; newer AppLoad releases may change its format.

## E-ink display, keyboard and landscape (AppLoad)

The protocol details below come from reading AppLoad's source (rm-appload: `src/qtfb/common.h`, `fbmanagement.cpp`, `FBController.cpp`, `libraryexternals.cpp`); the logic is unit tested in `tests/test_display.cpp`, but none of it has been run on a tablet yet.

- **Partial updates.** Each present compares the new frame with the last one sent and sends only the changed rectangle (padded by 16 px) as `UPDATE_PARTIAL`; the whole screen when most of it changed. Idle frames send nothing. While the pen is down, frames go out every 40 ms (every 250 ms when idle), so bond previews follow the pen.
- **Fast waveform (opt-in).** With `RECHEMDRAW_REFRESH_SWITCH=1` (set in `reChemDrawable.sh`), the first pen event switches AppLoad to the FAST waveform, and 1.2 s after the pen lifts the default (UI) waveform returns with one clean update of the drawn area. It is off by default because AppLoad sleeps one second after each refresh-mode message, so the first stroke after a pause could appear up to a second late; try it on the tablet and keep whichever feels better.
- **Clean screen** (top right) asks AppLoad for a full refresh to clear ghosting and resends the whole picture.
- **On-screen keyboard.** The manifest has `supportsVirtualKeyboard`; AppLoad then shows a keyboard button in the app's title bar. Tap it, tap the Text or Import SMILES field, and type. Key presses arrive as Qt key codes and are delivered to the focused field.
- **Landscape.** The manifest has `supportsRotation`; use AppLoad's rotate button on the window. AppLoad turns the picture itself and reports the rotation; the app then lays out at 2160x1620, draws into the portrait framebuffer rotated the opposite way and maps touches back. If the picture comes out upside down after a quarter turn, set `RECHEMDRAW_FLIP_ROTATION=1` in `reChemDrawable.sh` (the left/right direction is derived from AppLoad's painting code, not tested). In landscape the toolbar uses three columns.

Unverified until tried on the device: partial updates, whether the FAST waveform looks right for ink, the one-second stall after a mode switch, keyboard focus delivery under the offscreen platform, and the rotation direction.

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

## License

[MIT](LICENSE).

## Disclaimer

reChemDrawable is an independent community project. It is not affiliated with, endorsed by, or supported by reMarkable AS or Revvity. reMarkable and Paper Pro are trademarks of reMarkable AS; ChemDraw is a trademark of Revvity, Inc. Use it at your own risk; keep backups of anything important on your tablet.
