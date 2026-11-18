# reMarkable Paper Pro — ChemDraw-Style Structure Editor

Project handoff plan. Target device: reMarkable Paper Pro, dev mode enabled, XOVI + AppLoad already installed via remanager.

---

## 1. Goal

A native on-device app that lets the user build 2D skeletal chemical structures by tapping and dragging (atoms, bonds, ring templates), the way ChemDraw works on desktop — not a handwriting-recognition tool. No structures are recognized from ink; everything is placed as discrete vector objects from the start.

## 2. Scope decisions already made

- **Native app, not handwriting recognition.** No ML, no on-device inference (the hardware has no GPU and 2GB RAM, ruling that out anyway).
- **Standalone AppLoad app for v1.** Not an overlay inside the native notebook app. Overlay integration would require hooking the system UI via QML resource injection, a much bigger and riskier task — revisit only after the standalone version is useful.
- **No heavy chemistry library dependency.** No RDKit, no OpenBabel port. The app only needs to maintain a graph of atoms and bonds and serialize it to a standard file format (see Section 6). All "chemistry" beyond that (valence rules, canonical SMILES, 3D) is left to whatever software opens the exported file on a computer.
- **MVP has no live-drag bond preview.** Tap start atom, tap end atom, bond appears. Avoids needing to hook the device's fast-refresh ink pipeline for v1. Live preview is a stretch goal (Section 10, Stage 6).

## 3. Tools & toolchain

- **reMarkable Developer SDK** — official Qt Quick (QML) SDK with the EPaper QPA backend, used to build native apps that get proper e-ink rendering support instead of a generic Linux framebuffer.
- **CMake** — standard build system for reMarkable Qt Quick apps (`CMakeLists.txt` + `main.cpp` + a root `.qml` file is the standard skeleton reMarkable's own examples use).
- **Docker cross-compile image** — reMarkable's SDK targets are typically consumed through a Docker container that has the cross-compiler and vendored Qt libraries for the device's CPU (Paper Pro is **aarch64**, not the armv7 of the older reMarkable 2 — check this before reusing any older reMarkable 2 build script).
- **A local "PC" build target** — the same QML app built and run directly on the dev machine (desktop Qt, no cross-compile) for fast UI iteration without flashing the tablet every time. Build the real interaction logic and geometry here first; only cross-compile and push to hardware to check e-ink-specific behavior.
- **XOVI + AppLoad** (already installed) — the app is packaged as a folder under the device's AppLoad directory with a manifest file describing how to launch it. AppLoad hands the app direct access to the e-ink framebuffer when the manifest requests it.
- **git** — keep this as a real repo from the start, since work will move between this plan, a local AI assistant, and manual testing.

## 4. Repository structure

```
rmpp-chemdraw/
├── CMakeLists.txt
├── README.md
├── build-pc.sh                 # desktop build, for fast iteration
├── build-rmpp.sh                # aarch64 cross-build for Paper Pro
├── src/
│   ├── main.cpp                 # entry point, registers C++ types with the QML engine
│   ├── core/
│   │   ├── Atom.h / Atom.cpp
│   │   ├── Bond.h / Bond.cpp
│   │   ├── Molecule.h / Molecule.cpp        # graph container + mutation API
│   │   ├── GeometryEngine.h / .cpp          # angle snapping, ring template generation
│   │   ├── CommandStack.h / .cpp            # undo/redo
│   │   ├── MolfileWriter.h / .cpp           # Molecule -> .mol export
│   │   └── MolfileReader.h / .cpp           # .mol -> Molecule import (v2, for reopening saved files)
│   └── qml/
│       ├── Main.qml                  # window, toolbar, canvas layout
│       ├── MoleculeCanvas.qml        # renders the molecule, handles taps/drags
│       ├── Toolbar.qml               # tool selector: atom / bond / ring / erase / undo / save
│       ├── ElementPalette.qml        # heteroatom picker (O, N, S, halogens, P, etc.)
│       └── RingTemplatePicker.qml    # cyclohexane / cyclopentane / benzene / etc.
├── packaging/
│   ├── appload/
│   │   ├── external.manifest.json    # AppLoad manifest, requests framebuffer access
│   │   └── deploy-appload.sh         # copies build output + manifest to device over SSH
│   └── docker/
│       └── Dockerfile                # SDK cross-compile environment
└── test-molecules/
    └── *.mol                         # small hand-written .mol files for testing the renderer
```

## 5. Architecture

Two layers, talking through the Qt/QML property and signal system:

**Core (C++).** Owns the molecule graph, geometry math, undo history, and file I/O. Exposes the `Molecule` object (and a few helper functions) to QML as a registered type or context property. This layer has zero UI code and zero e-ink-specific code, so it's the part that can be unit-tested on a normal desktop with no device involved.

**UI (QML).** Renders whatever the `Molecule` object currently contains, and translates taps/drags into calls on the Core API (`addAtom`, `addBond`, `cycleBondOrder`, `deleteAtom`, `undo`, `redo`). The UI layer never mutates chemistry state directly — it only calls Core methods, so Core stays the single source of truth and stays testable independent of the canvas.

Data flow for a typical action (draw a bond):
1. User taps atom A, then atom B, on `MoleculeCanvas`.
2. QML calls `molecule.addBond(idA, idB)`.
3. `Molecule::addBond` asks `GeometryEngine` to validate/snap the implied angle if one endpoint is a new atom, pushes an entry onto `CommandStack`, updates internal state.
4. `Molecule` emits a changed signal.
5. `MoleculeCanvas` redraws only the affected region.

## 6. Data model

Keep this deliberately small. A skeletal structure is just a graph.

```cpp
struct Atom {
    int id;
    QString element;   // "C" is default/implicit in skeletal notation; label only heteroatoms
    double x, y;        // canvas coordinates
};

struct Bond {
    int atomA, atomB;
    int order;           // 1 = single, 2 = double, 3 = triple
    int stereo;           // 0 = none, 1 = wedge, 2 = dash (v2 feature, not MVP)
};

class Molecule {
    QVector<Atom> atoms;
    QVector<Bond> bonds;
    // addAtom, addBond, removeAtom, removeBond,
    // cycleBondOrder(bondId), atomAt(x, y) for hit-testing,
    // toMolfile(), loadFromMolfile()
};
```

**Export format: MDL Molfile (V2000).** This is a plain-text, decades-old, well-documented format: a header, a counts line, an atom block (one line per atom with x/y/z and element symbol), and a bond block (one line per bond with the two atom indices and bond order). Any real chemistry software (RDKit, PubChem, actual ChemDraw) can open it. Writing it is a few dozen lines of string formatting — no library needed. If precise formatting details are needed while implementing `MolfileWriter`, the "MDL Molfile V2000 format" is a standard, searchable specification.

## 7. Interaction / tool design

Toolbar tools, left to right:

- **Atom/bond tool (default).** Tap empty canvas: places a new unlabeled atom (implicit carbon). Tap an existing atom then tap a second point: draws a bond, creating a new atom at the second point if it's not on an existing one. When the second point is a new atom being placed off an existing atom, snap the bond angle to a fixed set of increments (e.g. every 30°) so chains and rings come out as clean zigzags rather than freehand-angle segments.
- **Bond order.** Tap an existing bond to cycle single → double → triple → back to single.
- **Ring templates.** Opens `RingTemplatePicker`; tapping a template (cyclohexane, cyclopentane, benzene/aromatic — alternating double bonds, cyclobutane, cyclopropane) then tapping the canvas stamps a pre-built ring subgraph at that point.
- **Element palette.** Select an atom, open `ElementPalette`, tap an element to relabel that atom (O, N, S, F, Cl, Br, I, P cover the large majority of intro/organic chemistry needs). No text keyboard in v1.
- **Eraser/delete.** Tap an atom or bond to remove it (removing an atom removes its bonds too).
- **Undo/redo.** Backed by `CommandStack`.
- **Save/export.** Writes the current `Molecule` to a `.mol` file in a known folder on-device.

## 8. Rendering and e-ink considerations

This workload is discrete-update (tap → redraw), the same pattern already used successfully by simple existing community apps on this device (calculators, readers). That's the easy case for e-ink and needs no special handling beyond normal partial-refresh redraw of the changed region.

The one genuinely hard case is a **live preview while dragging** a bond before release — that needs low-latency partial refresh closer to what the device's own handwriting engine uses, not a generic QML repaint. This is achievable (some community apps already hook into that lower-level rendering path) but is meaningfully more complex and higher-risk to get right. **Treat it as Stage 6, not part of the MVP.**

## 9. Build & deploy workflow

1. Iterate on Core logic and QML layout using the desktop/PC build (`build-pc.sh`), no device involved.
2. Write small `.mol` test files by hand in `test-molecules/` to validate the renderer draws correct bond lengths, angles, and double-bond offset lines before any interactive editing is wired up.
3. Once behavior looks right on desktop, cross-compile for aarch64 (`build-rmpp.sh`, via the Docker SDK image).
4. Package the build output into `packaging/appload/` alongside `external.manifest.json` (must declare framebuffer access so AppLoad hands the app a full-screen e-ink surface).
5. Deploy over SSH to the device (dev mode's SSH access, same connection remanager/AppLoad already use) via `deploy-appload.sh`.
6. Refresh AppLoad's launcher on-device and open the app to test.

## 10. Staged roadmap

1. **Toolchain bring-up.** Get a trivial "hello world" QML app running in the PC build, then confirm the same build cross-compiles and runs on the actual tablet via AppLoad. No app logic yet — just prove the pipeline end to end.
2. **Static renderer.** Hand-write a couple of `.mol` test files, load them with `MolfileReader`, and get `MoleculeCanvas` to draw correct skeletal geometry: bond lengths, angles, parallel offset lines for double/triple bonds. This is the one piece that's genuinely chemistry-specific; everything after this is mostly UI wiring.
3. **Basic editing.** Tap-to-add atoms, tap-tap-to-add bonds (no drag preview), `CommandStack`-backed undo/redo.
4. **Bond order + element palette.** Cycling bond order on tap; heteroatom relabeling via `ElementPalette`.
5. **Ring templates.** `RingTemplatePicker` plus the pre-built ring subgraphs in `GeometryEngine`.
6. **Export + save location.** `MolfileWriter`, decide where files land on-device (Section 11).
7. **Stretch goals.** Live-drag bond preview via the fast-refresh ink path; wedge/dash stereo bonds; functional-group stamps (OH, NH2, COOH, etc.); in-notebook overlay integration instead of standalone-only.

## 11. Open decisions

- **Save location.** Just on-device storage, or feed exported `.mol` files into an existing sync-to-laptop workflow?
- **Ring template set for v1.** Just the 3 or 4 most common rings, or a broader library from the start?
- **Distribution.** Personal use only (manual deploy via SSH each time) vs. packaging it for the AppLoad catalog for others to install — affects how much polish the manifest and packaging need.
- **Angle snap increment.** 15°, 30°, or something else — affects how natural chain zigzags look versus how forgiving the tap targets are on a small screen.

## 12. Risks & mitigations

- **E-ink latency on interactive drag.** Mitigated by deferring live preview to a later stage (Section 10, Stage 6) and shipping a tap-only MVP first.
- **SDK/XOVI/AppLoad breakage on firmware updates.** This ecosystem is community-maintained and Paper Pro support in some tools is newer than reMarkable 2 support. Check the current compatibility state of whatever XOVI/AppLoad version is on the device before each significant build, especially after any reMarkable firmware update.
- **Cross-compile toolchain friction.** Keep dependencies to the C++ standard library and Qt only — no third-party chemistry libraries — specifically to avoid fighting the cross-compile toolchain over dependencies that were never built for aarch64 e-ink devices.
- **Text input UX on e-ink.** Avoided in v1 by using tap palettes (`ElementPalette`, `RingTemplatePicker`) instead of any on-screen keyboard flow.

## 13. Reference material

- reMarkable's official developer documentation (developer.remarkable.com) — developer mode setup, the Qt Quick e-paper app-writing guide, and an official example repo (`reMarkable/remarkable-developer-examples`) containing a working calculator app for both reMarkable 2 and Paper Pro. Good starting skeleton for the CMake/main.cpp/QML structure.
- `reHackable/awesome-reMarkable` on GitHub — curated list of community tools and file-format references for the reMarkable ecosystem.
- XOVI extension framework and AppLoad — the sideloading mechanism already installed on this device via remanager.
- MDL Molfile (V2000) format — standard, publicly documented chemical file format used for export in Section 6.
