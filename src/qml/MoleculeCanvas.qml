import QtQuick

Rectangle {
    id: root
    property string activeTool: "draw"
    property string ringTemplate: "cyclohexane"
    property int selectedAtom: -1
    property var selectedAtoms: []
    property bool boxSelectionCommitted: false
    property real zoom: 1.0
    // View setting (not saved): thicker bonds and larger labels for e-ink readability.
    property bool boldLines: false
    readonly property real bondWidth: boldLines ? 4.5 : 3
    readonly property real labelScale: boldLines ? 1.2 : 1.0
    // Freeform lasso drawn by the Select tool, in canvas (screen) coordinates.
    property var lassoPoints: []
    property real panX: 0
    property real panY: 0
    property bool gridSnapEnabled: false
    property bool stereoLabelsVisible: false
    onStereoLabelsVisibleChanged: drawing.requestPaint()
    property var atomItems: []
    property var bondItems: []
    property var reactionItems: []
    property bool bondPreviewVisible: false
    property int bondPreviewAtom: -1
    property real bondPreviewX: 0
    property real bondPreviewY: 0
    property string labelText: "conditions"
    // forward, equilibrium, retro, resonance, dashed, noreaction, curved, fishhook
    property string arrowKind: "forward"
    // Pen path of an arrow drag in progress, in model coordinates.
    property var arrowPreviewPath: []
    // Chain tool: live preview of the zig-zag being dragged out.
    property var chainPreviewPoints: []
    property string chainPreviewLabel: ""
    property real chainPreviewLabelX: 0
    property real chainPreviewLabelY: 0
    signal selectedAtomChangedByCanvas(int atomId)
    signal status(string text)

    color: "white"
    border.color: "#7b7a74"
    border.width: 2
    radius: 4
    clip: true

    function refresh() {
        atomItems = molecule.atoms()
        bondItems = molecule.bonds()
        reactionItems = reactionLayer.items()
        // Drop selected ids that no longer exist (after erase, undo, clear, or load).
        const present = {}
        for (let i = 0; i < atomItems.length; ++i) present[atomItems[i].id] = true
        const kept = selectedAtoms.filter(function(id) { return present[id] === true })
        if (kept.length !== selectedAtoms.length) selectedAtoms = kept
        if (selectedAtom >= 0 && !present[selectedAtom]) selectedAtomChangedByCanvas(-1)
        drawing.requestPaint()
    }

    // Draws one reaction arrow; the SVG writer mirrors these shapes.
    function paintArrow(ctx, item) {
        const dx = item.x2 - item.x1, dy = item.y2 - item.y1
        const length = Math.sqrt(dx * dx + dy * dy)
        if (length < 0.01) return
        const ux = dx / length, uy = dy / length, nx = -uy, ny = ux
        const kind = item.kind || "forward"
        const seg = function(x1, y1, x2, y2) { ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke() }
        // side 0: both barbs; +1 / -1: only the barb on that side (half head).
        const head = function(tx, ty, hx, hy, side, size, spread) {
            const sz = size || 13, sp = spread || 7
            const bx = -hy, by = hx
            ctx.beginPath()
            if (side >= 0) { ctx.moveTo(tx - hx * sz + bx * sp, ty - hy * sz + by * sp); ctx.lineTo(tx, ty) }
            else ctx.moveTo(tx, ty)
            if (side <= 0) ctx.lineTo(tx - hx * sz - bx * sp, ty - hy * sz - by * sp)
            ctx.stroke()
        }
        if (kind === "curved" || kind === "fishhook") {
            const cx = item.cx, cy = item.cy
            let tx = item.x2 - cx, ty = item.y2 - cy
            const tl = Math.sqrt(tx * tx + ty * ty)
            if (tl < 0.01) { tx = ux; ty = uy } else { tx /= tl; ty /= tl }
            ctx.beginPath(); ctx.moveTo(item.x1, item.y1); ctx.quadraticCurveTo(cx, cy, item.x2, item.y2); ctx.stroke()
            head(item.x2, item.y2, tx, ty, kind === "fishhook" ? 1 : 0)
        } else if (kind === "equilibrium") {
            // ⇌: the upper line points forward, the lower one back, each with its
            // half barb on the outside. With y down, the -n side is the upper one.
            seg(item.x1 - nx * 4, item.y1 - ny * 4, item.x2 - nx * 4, item.y2 - ny * 4)
            head(item.x2 - nx * 4, item.y2 - ny * 4, ux, uy, -1)
            seg(item.x2 + nx * 4, item.y2 + ny * 4, item.x1 + nx * 4, item.y1 + ny * 4)
            head(item.x1 + nx * 4, item.y1 + ny * 4, -ux, -uy, -1)
        } else if (kind === "retro") {
            const back = Math.min(length * 0.5, 14)
            seg(item.x1 + nx * 4, item.y1 + ny * 4, item.x2 - ux * back + nx * 4, item.y2 - uy * back + ny * 4)
            seg(item.x1 - nx * 4, item.y1 - ny * 4, item.x2 - ux * back - nx * 4, item.y2 - uy * back - ny * 4)
            head(item.x2, item.y2, ux, uy, 0, 16, 11)
        } else if (kind === "resonance") {
            seg(item.x1, item.y1, item.x2, item.y2)
            head(item.x2, item.y2, ux, uy, 0)
            head(item.x1, item.y1, -ux, -uy, 0)
        } else {
            if (kind === "dashed") ctx.setLineDash([9, 6])
            seg(item.x1, item.y1, item.x2, item.y2)
            ctx.setLineDash([])
            head(item.x2, item.y2, ux, uy, 0)
            if (kind === "noreaction") {
                const mx = (item.x1 + item.x2) / 2, my = (item.y1 + item.y2) / 2
                seg(mx - (ux + nx) * 9, my - (uy + ny) * 9, mx + (ux + nx) * 9, my + (uy + ny) * 9)
                seg(mx - (ux - nx) * 9, my - (uy - ny) * 9, mx + (ux - nx) * 9, my + (uy - ny) * 9)
            }
        }
    }

    // A half-placed arrow should not survive switching to another tool.
    onActiveToolChanged: canvasInput.awaitingArrowEnd = false

    function atomById(id) {
        for (let i = 0; i < atomItems.length; ++i)
            if (atomItems[i].id === id) return atomItems[i]
        return null
    }

    function atomColor(element) {
        switch (element) {
        case "O": return "#d12f28"
        case "N": return "#1769c2"
        case "S": return "#b78a00"
        case "P": return "#d06b16"
        case "F": case "Cl": return "#148542"
        case "Br": return "#8b3f20"
        case "I": return "#7045a8"
        case "B": return "#b0565a"
        case "Si": return "#8a6d3b"
        case "Se": return "#a05a00"
        case "Li": case "Na": case "K": case "Mg": return "#5b3fa0"
        default: return "#252525"
        }
    }

    function benzeneCenterForBond(seedBond) {
        let ids = [seedBond.atomA, seedBond.atomB]
        let cursor = 0
        while (cursor < ids.length) {
            const atomId = ids[cursor++]
            for (let index = 0; index < bondItems.length; ++index) {
                const bond = bondItems[index]
                if (bond.style !== "benzene") continue
                if (bond.atomA === atomId && ids.indexOf(bond.atomB) < 0) ids.push(bond.atomB)
                if (bond.atomB === atomId && ids.indexOf(bond.atomA) < 0) ids.push(bond.atomA)
            }
        }
        let center = { x: 0, y: 0 }
        for (let index = 0; index < ids.length; ++index) {
            const atom = atomById(ids[index])
            if (atom) { center.x += atom.x; center.y += atom.y }
        }
        if (ids.length) { center.x /= ids.length; center.y /= ids.length }
        return center
    }

    // One bond length in canvas units, from GeometryEngine::StandardBondLength.
    readonly property real bondLength: molecule.standardBondLength

    // Triangular lattice whose neighbouring points sit one bond length apart at
    // +-30 and 90 degrees, so zig-zag chains and hexagons land on grid points.
    // Columns are sqrt(3)/2 bond lengths apart; odd columns shift half a bond.
    function snapToHexGrid(x, y) {
        const columnWidth = bondLength * Math.sqrt(3) / 2
        const column = Math.round(x / columnWidth)
        const offset = Math.abs(column) % 2 ? bondLength / 2 : 0
        const row = Math.round((y - offset) / bondLength)
        return { x: column * columnWidth, y: row * bondLength + offset }
    }

    // Chain parameters for a drag from (startX, startY) (model coordinates, or
    // atom startAtom) along `samples` (the pen path in model coordinates).
    // Grid snap restricts the axis to multiples of 60 degrees, whose +-30 degree
    // zig-zag bonds all lie on the triangular lattice.
    function chainParameters(startAtom, startX, startY, samples) {
        const last = samples[samples.length - 1]
        const dx = last.x - startX, dy = last.y - startY
        let angle = Math.atan2(dy, dx) * 180 / Math.PI
        if (gridSnapEnabled) angle = Math.round(angle / 60) * 60
        const along = Math.max(0, dx * Math.cos(angle * Math.PI / 180) + dy * Math.sin(angle * Math.PI / 180))
        const step = bondLength * Math.cos(Math.PI / 6)
        const bonds = Math.round(along / step)
        // Side the path bulges toward (+1 is clockwise on screen); default upward.
        const chord = Math.sqrt(dx * dx + dy * dy)
        let deviation = 0
        for (let i = 0; i < samples.length && chord > 0; ++i) {
            const d = (dx * (samples[i].y - startY) - dy * (samples[i].x - startX)) / chord
            if (Math.abs(d) > Math.abs(deviation)) deviation = d
        }
        let sign = 1
        if (Math.abs(deviation) > 8 / zoom) sign = deviation > 0 ? 1 : -1
        else sign = Math.sin((angle - 30) * Math.PI / 180) < Math.sin((angle + 30) * Math.PI / 180) ? -1 : 1
        return { angle: angle, count: bonds < 1 ? 0 : (startAtom >= 0 ? bonds : bonds + 1), sign: sign }
    }

    function placementPoint(x, y) {
        return gridSnapEnabled ? snapToHexGrid(x, y) : { x: x, y: y }
    }

    function suggestedBondEnd(atomId) {
        const point = molecule.suggestedBondEnd(atomId)
        return { x: point.x, y: point.y }
    }

    function modelPoint(x, y) {
        return { x: (x - panX) / zoom, y: (y - panY) / zoom }
    }

    readonly property real minimumZoom: 0.35
    readonly property real maximumZoom: 3.0

    function zoomAt(factor, x, y) {
        zoomTo(zoom * factor, x, y)
    }

    // Sets the zoom while keeping the model point under (x, y) in place.
    function zoomTo(value, x, y) {
        const before = modelPoint(x, y)
        zoom = Math.max(minimumZoom, Math.min(maximumZoom, value))
        panX = x - before.x * zoom
        panY = y - before.y * zoom
        drawing.requestPaint()
    }

    // Two-finger pan and pinch from the tablet: zoom by `scale` about (cx, cy)
    // in canvas coordinates (clamped to the zoom limits), then pan by (dx, dy).
    function panZoomBy(dx, dy, scale, cx, cy) {
        if (scale !== 1) zoomTo(zoom * scale, cx, cy)
        panX += dx
        panY += dy
        drawing.requestPaint()
    }

    function zoomIn() { zoomAt(1.25, width / 2, height / 2) }
    function zoomOut() { zoomAt(1 / 1.25, width / 2, height / 2) }

    // Centers the drawing and zooms so all of it fits, up to 1:1 for small drawings.
    function fitToContent() {
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity
        const include = function(x, y) {
            minX = Math.min(minX, x); maxX = Math.max(maxX, x)
            minY = Math.min(minY, y); maxY = Math.max(maxY, y)
        }
        for (let i = 0; i < atomItems.length; ++i) include(atomItems[i].x, atomItems[i].y)
        for (let i = 0; i < reactionItems.length; ++i) {
            include(reactionItems[i].x1, reactionItems[i].y1)
            include(reactionItems[i].x2, reactionItems[i].y2)
            if (reactionItems[i].type === "arrow" && (reactionItems[i].kind === "curved" || reactionItems[i].kind === "fishhook"))
                include((reactionItems[i].x1 + 2 * reactionItems[i].cx + reactionItems[i].x2) / 4,
                        (reactionItems[i].y1 + 2 * reactionItems[i].cy + reactionItems[i].y2) / 4)
        }
        if (minX === Infinity) {
            zoom = 1; panX = 0; panY = 0
            drawing.requestPaint()
            return
        }
        const margin = bondLength
        const contentWidth = maxX - minX + 2 * margin, contentHeight = maxY - minY + 2 * margin
        zoom = Math.max(minimumZoom, Math.min(1.0, width / contentWidth, height / contentHeight))
        panX = width / 2 - (minX + maxX) / 2 * zoom
        panY = height / 2 - (minY + maxY) / 2 * zoom
        drawing.requestPaint()
    }

    function selectOnly(atomId) {
        selectedAtoms = atomId >= 0 ? [atomId] : []
        selectedAtomChangedByCanvas(atomId)
        drawing.requestPaint()
    }

    function toggleSelection(atomId) {
        if (atomId < 0) return
        const index = selectedAtoms.indexOf(atomId)
        selectedAtoms = index >= 0 ? selectedAtoms.filter(function(id) { return id !== atomId }) : selectedAtoms.concat([atomId])
        selectedAtomChangedByCanvas(selectedAtoms.length ? selectedAtoms[selectedAtoms.length - 1] : -1)
        drawing.requestPaint()
    }

    // Removes the arrow or label, atom, or bond under (x, y) in model coordinates.
    function eraseAt(x, y) {
        const item = reactionLayer.itemAt(x, y, 16 / zoom)
        if (item >= 0) return reactionLayer.removeItem(item)
        const atom = molecule.atomAt(x, y, 22 / zoom)
        if (atom >= 0) return molecule.removeAtom(atom)
        const bond = molecule.bondAt(x, y, 16 / zoom)
        if (bond >= 0) return molecule.removeBond(bond)
        return false
    }

    // Erases along a stroke segment, sampling often enough that a quick
    // stroke does not skip over a bond.
    function eraseAlong(x1, y1, x2, y2) {
        const steps = Math.max(1, Math.ceil(Math.sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) / (8 / zoom)))
        for (let i = 1; i <= steps; ++i) eraseAt(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps)
    }

    function rotateSelection() {
        if (selectedAtoms.length) molecule.rotateAtoms(selectedAtoms, 30)
    }

    function mirrorSelection(horizontal) {
        if (selectedAtoms.length && molecule.mirrorAtoms(selectedAtoms, horizontal)) status("Selected fragment mirrored.")
        else status("Select one or more atoms to mirror.")
    }

    function alignSelection(alignment) {
        if (molecule.alignAtoms(selectedAtoms, alignment)) status("Selected atoms aligned.")
        else status("Select at least two atoms to align.")
    }

    function deleteSelection() {
        if (selectedAtoms.length) molecule.removeAtoms(selectedAtoms)
        selectedAtoms = []
        selectedAtomChangedByCanvas(-1)
    }

    function copySelection() {
        if (selectedAtoms.length && molecule.copyAtoms(selectedAtoms)) status("Fragment copied.")
        else status("Select one or more atoms to copy.")
    }

    // Copies the selected fragment two bond lengths down-right in one undo step
    // and selects the copy.
    function duplicateSelection() {
        const offset = 2 * bondLength
        const created = selectedAtoms.length ? molecule.duplicateAtoms(selectedAtoms, offset, offset) : []
        if (created.length) {
            selectedAtoms = created
            selectedAtomChangedByCanvas(created[created.length - 1])
            status("Fragment duplicated.")
        } else {
            status("Select one or more atoms to duplicate.")
        }
        drawing.requestPaint()
    }

    function pasteSelection() {
        const target = modelPoint(width / 2, height / 2)
        const pasted = molecule.pasteAtoms(target.x, target.y)
        if (pasted.length) {
            selectedAtoms = pasted
            selectedAtomChangedByCanvas(pasted[pasted.length - 1])
            status("Fragment pasted at canvas center.")
        }
    }

    Canvas {
        id: drawing
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "white"
            ctx.fillRect(0, 0, width, height)
            if (root.lassoPoints.length > 1) {
                ctx.strokeStyle = "#756100"
                ctx.lineWidth = 2
                ctx.setLineDash([4, 3])
                ctx.beginPath()
                ctx.moveTo(root.lassoPoints[0].x, root.lassoPoints[0].y)
                for (let i = 1; i < root.lassoPoints.length; ++i) ctx.lineTo(root.lassoPoints[i].x, root.lassoPoints[i].y)
                ctx.stroke()
                ctx.setLineDash([])
            }
            ctx.save()
            ctx.translate(root.panX, root.panY)
            ctx.scale(root.zoom, root.zoom)
            if (root.gridSnapEnabled) {
                // Same lattice as snapToHexGrid.
                const spacing = root.bondLength
                const columnWidth = spacing * Math.sqrt(3) / 2
                const minX = -root.panX / root.zoom
                const maxX = (width - root.panX) / root.zoom
                const minY = -root.panY / root.zoom
                const maxY = (height - root.panY) / root.zoom
                ctx.fillStyle = "#e4e1d8"
                const firstColumn = Math.floor(minX / columnWidth) - 1
                const lastColumn = Math.ceil(maxX / columnWidth) + 1
                for (let column = firstColumn; column <= lastColumn; ++column) {
                    const offset = Math.abs(column) % 2 ? spacing / 2 : 0
                    const firstRow = Math.floor((minY - offset) / spacing) - 1
                    const lastRow = Math.ceil((maxY - offset) / spacing) + 1
                    for (let row = firstRow; row <= lastRow; ++row) {
                        const gridX = column * columnWidth
                        const gridY = row * spacing + offset
                        ctx.fillRect(gridX - 1 / root.zoom, gridY - 1 / root.zoom,
                                     2 / root.zoom, 2 / root.zoom)
                    }
                }
            }
            ctx.strokeStyle = "#121212"
            ctx.fillStyle = "#121212"
            ctx.lineWidth = root.bondWidth
            for (let i = 0; i < reactionItems.length; ++i) {
                const item = reactionItems[i]
                if (item.type === "arrow") {
                    root.paintArrow(ctx, item)
                } else if (item.type === "label") {
                    ctx.font = (item.bold ? "bold " : "") + Math.round(18 * root.labelScale) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    ctx.fillText(item.text, item.x1, item.y1)
                }
            }
            if (arrowPreviewPath.length > 1) {
                // Live ink of the arrow being dragged.
                ctx.lineWidth = 2 / root.zoom
                ctx.beginPath()
                ctx.moveTo(arrowPreviewPath[0].x, arrowPreviewPath[0].y)
                for (let i = 1; i < arrowPreviewPath.length; ++i) ctx.lineTo(arrowPreviewPath[i].x, arrowPreviewPath[i].y)
                ctx.stroke()
                ctx.lineWidth = root.bondWidth
            }
            for (let i = 0; i < bondItems.length; ++i) {
                const bond = bondItems[i]
                const a = root.atomById(bond.atomA)
                const b = root.atomById(bond.atomB)
                if (!a || !b) continue
                const dx = b.x - a.x, dy = b.y - a.y
                const length = Math.sqrt(dx * dx + dy * dy)
                const nx = length > 0 ? -dy / length : 0
                const ny = length > 0 ? dx / length : 0
                if (bond.style === "wedge") {
                    ctx.beginPath()
                    ctx.moveTo(a.x, a.y)
                    ctx.lineTo(b.x + nx * 8, b.y + ny * 8)
                    ctx.lineTo(b.x - nx * 8, b.y - ny * 8)
                    ctx.closePath()
                    ctx.fill()
                    continue
                }
                if (bond.style === "dash") {
                    for (let dash = 1; dash < 7; ++dash) {
                        const progress = dash / 7
                        const px = a.x + dx * progress, py = a.y + dy * progress
                        const width = 1 + progress * 8
                        ctx.beginPath(); ctx.moveTo(px + nx * width, py + ny * width)
                        ctx.lineTo(px - nx * width, py - ny * width); ctx.stroke()
                    }
                    continue
                }
                if (bond.style === "wavy") {
                    ctx.beginPath(); ctx.moveTo(a.x, a.y)
                    for (let wave = 1; wave <= 6; ++wave) {
                        const progress = wave / 6
                        const px = a.x + dx * progress, py = a.y + dy * progress
                        const sign = wave % 2 === 0 ? -1 : 1
                        ctx.quadraticCurveTo(px + nx * sign * 7, py + ny * sign * 7, px, py)
                    }
                    ctx.stroke(); continue
                }
                if (bond.style === "bold") {
                    // Haworth front edge: a thick line.
                    ctx.lineWidth = root.bondWidth * 2.3
                    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke()
                    ctx.lineWidth = root.bondWidth
                    continue
                }
                if (bond.style === "newman") {
                    // Newman projection: the circle for the rear carbon.
                    ctx.beginPath()
                    ctx.arc((a.x + b.x) / 2, (a.y + b.y) / 2, molecule.newmanCircleRadius, 0, 2 * Math.PI)
                    ctx.stroke()
                    continue
                }
                if (bond.style === "newmanback") {
                    // Rear bonds of a Newman projection start at the circle.
                    const inset = length > 0 ? Math.min(molecule.newmanCircleRadius, length) / length : 0
                    ctx.beginPath(); ctx.moveTo(a.x + dx * inset, a.y + dy * inset); ctx.lineTo(b.x, b.y); ctx.stroke()
                    continue
                }
                if (bond.style === "benzene") {
                    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke()
                    if (bond.order === 2 && length > 0) {
                        const center = root.benzeneCenterForBond(bond)
                        let inwardX = nx, inwardY = ny
                        const midpointX = (a.x + b.x) / 2, midpointY = (a.y + b.y) / 2
                        if ((center.x - midpointX) * inwardX + (center.y - midpointY) * inwardY < 0) {
                            inwardX = -inwardX; inwardY = -inwardY
                        }
                        const ux = dx / length, uy = dy / length
                        ctx.beginPath()
                        ctx.moveTo(a.x + ux * 8 + inwardX * 7, a.y + uy * 8 + inwardY * 7)
                        ctx.lineTo(b.x - ux * 8 + inwardX * 7, b.y - uy * 8 + inwardY * 7)
                        ctx.stroke()
                    }
                    continue
                }
                if (bond.order === 2 && bond.doubleSide !== 0 && bond.style === "plain" && length > 0) {
                    // Full line on the atom axis plus a shorter parallel line to one side.
                    const inset = bond.doubleSide > 0 ? 7 : -7
                    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(a.x + dx * 0.12 + nx * inset, a.y + dy * 0.12 + ny * inset)
                    ctx.lineTo(b.x - dx * 0.12 + nx * inset, b.y - dy * 0.12 + ny * inset)
                    ctx.stroke()
                    continue
                }
                const offsets = bond.style === "aromatic" ? [0] : bond.order === 1 ? [0] : bond.order === 2 ? [-4, 4] : [-6, 0, 6]
                if (bond.style === "aromatic") ctx.setLineDash([7, 5])
                for (let j = 0; j < offsets.length; ++j) {
                    ctx.beginPath()
                    ctx.moveTo(a.x + nx * offsets[j], a.y + ny * offsets[j])
                    ctx.lineTo(b.x + nx * offsets[j], b.y + ny * offsets[j])
                    ctx.stroke()
                }
                ctx.setLineDash([])
            }
            if (root.bondPreviewVisible) {
                const previewStart = root.atomById(root.bondPreviewAtom)
                if (previewStart) {
                    ctx.strokeStyle = "#4d7da8"
                    ctx.lineWidth = 2
                    ctx.setLineDash([7, 5])
                    ctx.beginPath(); ctx.moveTo(previewStart.x, previewStart.y)
                    ctx.lineTo(root.bondPreviewX, root.bondPreviewY); ctx.stroke()
                    ctx.setLineDash([])
                    ctx.strokeStyle = "#121212"
                    ctx.lineWidth = root.bondWidth
                }
            }
            if (root.chainPreviewPoints.length > 1) {
                ctx.strokeStyle = "#4d7da8"
                ctx.lineWidth = 2
                ctx.setLineDash([7, 5])
                ctx.beginPath()
                ctx.moveTo(root.chainPreviewPoints[0].x, root.chainPreviewPoints[0].y)
                for (let k = 1; k < root.chainPreviewPoints.length; ++k)
                    ctx.lineTo(root.chainPreviewPoints[k].x, root.chainPreviewPoints[k].y)
                ctx.stroke()
                ctx.setLineDash([])
                ctx.strokeStyle = "#121212"
                ctx.lineWidth = root.bondWidth
            }
            if (root.chainPreviewLabel !== "") {
                ctx.font = "bold 22px sans-serif"
                ctx.textAlign = "left"
                ctx.textBaseline = "middle"
                ctx.fillStyle = "#4d7da8"
                ctx.fillText(root.chainPreviewLabel, root.chainPreviewLabelX + 18 / root.zoom, root.chainPreviewLabelY - 22 / root.zoom)
                ctx.fillStyle = "#121212"
            }
            ctx.font = Math.round(20 * root.labelScale) + "px sans-serif"
            ctx.textAlign = "left"
            ctx.textBaseline = "middle"
            for (let i = 0; i < atomItems.length; ++i) {
                const atom = atomItems[i]
                let labelRight = atom.x + 7
                if (atom.showLabel) {
                    ctx.font = Math.round(20 * root.labelScale) + "px sans-serif"
                    ctx.textAlign = "left"
                    const text = atom.labelText
                    const before = ctx.measureText(text.substring(0, atom.centerIndex)).width
                    const center = ctx.measureText(text.substring(atom.centerIndex, atom.centerIndex + atom.centerLength)).width
                    const total = ctx.measureText(text).width
                    const left = atom.x - before - center / 2
                    ctx.fillStyle = "white"
                    ctx.fillRect(left - 3, atom.y - 14 * root.labelScale, total + 6, 28 * root.labelScale)
                    ctx.fillStyle = root.atomColor(atom.element)
                    ctx.fillText(text, left, atom.y)
                    labelRight = left + total + 1
                } else if (atom.element === "C") {
                    ctx.fillStyle = root.atomColor(atom.element)
                    ctx.beginPath(); ctx.arc(atom.x, atom.y, 4, 0, 2 * Math.PI); ctx.fill()
                }
                if (atom.charge !== 0) {
                    ctx.fillStyle = root.atomColor(atom.element)
                    ctx.strokeStyle = root.atomColor(atom.element)
                    if (molecule.circledCharges) {
                        const magnitude = Math.abs(atom.charge)
                        let cx = labelRight + 6
                        if (magnitude > 1) {
                            ctx.font = "12px sans-serif"; ctx.textAlign = "left"
                            ctx.fillText(String(magnitude), labelRight, atom.y - 14)
                            cx += 7
                        }
                        const cy = atom.y - 12
                        ctx.lineWidth = 1.5
                        ctx.beginPath(); ctx.arc(cx, cy, 6, 0, 2 * Math.PI); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(cx - 3.5, cy); ctx.lineTo(cx + 3.5, cy)
                        if (atom.charge > 0) { ctx.moveTo(cx, cy - 3.5); ctx.lineTo(cx, cy + 3.5) }
                        ctx.stroke()
                        ctx.lineWidth = root.bondWidth
                    } else {
                        ctx.font = "bold 15px sans-serif"; ctx.textAlign = "left"
                        ctx.fillText(atom.chargeLabel, labelRight, atom.y - 11)
                    }
                    ctx.font = Math.round(20 * root.labelScale) + "px sans-serif"
                    ctx.strokeStyle = "#121212"
                }
                const dotRadius = atom.showLabel ? 17 : 10
                ctx.fillStyle = root.atomColor(atom.element)
                for (let k = 0; k < atom.lonePairAngles.length; ++k) {
                    const angle = atom.lonePairAngles[k] * Math.PI / 180
                    const px = atom.x + dotRadius * Math.cos(angle), py = atom.y + dotRadius * Math.sin(angle)
                    const ox = -Math.sin(angle) * 3.5, oy = Math.cos(angle) * 3.5
                    ctx.beginPath(); ctx.arc(px + ox, py + oy, 2, 0, 2 * Math.PI); ctx.fill()
                    ctx.beginPath(); ctx.arc(px - ox, py - oy, 2, 0, 2 * Math.PI); ctx.fill()
                }
                for (let k = 0; k < atom.radicalAngles.length; ++k) {
                    const angle = atom.radicalAngles[k] * Math.PI / 180
                    ctx.beginPath(); ctx.arc(atom.x + dotRadius * Math.cos(angle), atom.y + dotRadius * Math.sin(angle), 2.5, 0, 2 * Math.PI); ctx.fill()
                }
                if (atom.problem) {
                    ctx.strokeStyle = "#e02020"
                    ctx.lineWidth = 3
                    ctx.beginPath(); ctx.arc(atom.x, atom.y, 21, 0, 2 * Math.PI); ctx.stroke()
                    ctx.strokeStyle = "#121212"; ctx.lineWidth = root.bondWidth
                }
                if (selectedAtoms.indexOf(atom.id) >= 0 || atom.id === selectedAtom) {
                    ctx.strokeStyle = "#756100"
                    ctx.lineWidth = 2
                    ctx.beginPath(); ctx.arc(atom.x, atom.y, 16, 0, 2 * Math.PI); ctx.stroke()
                    ctx.strokeStyle = "#121212"; ctx.lineWidth = root.bondWidth
                }
            }
            if (root.stereoLabelsVisible) {
                const stereo = molecule.stereoDescriptors()
                ctx.font = "italic 17px sans-serif"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                for (let s = 0; s < stereo.length; ++s) {
                    const metrics = ctx.measureText(stereo[s].text)
                    ctx.fillStyle = "white"
                    ctx.fillRect(stereo[s].x - metrics.width / 2 - 2, stereo[s].y - 10, metrics.width + 4, 20)
                    ctx.fillStyle = "#8a1f1f"
                    ctx.fillText(stereo[s].text, stereo[s].x, stereo[s].y)
                }
            }
            ctx.restore()
        }
    }

    MouseArea {
        id: canvasInput
        objectName: "canvasInput"
        anchors.fill: parent
        // The right button is the pen's eraser end (main.cpp sends eraser strokes
        // that way); on desktop, right-drag erases too.
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property bool erasing: false
        property real eraseLastX: 0
        property real eraseLastY: 0
        property int dragAtom: -1
        property int dragReactionItem: -1
        property real dragStartX: 0
        property real dragStartY: 0
        property real atomStartX: 0
        property real atomStartY: 0
        property real modelStartX: 0
        property real modelStartY: 0
        property var dragOriginalPositions: ({})
        property bool awaitingArrowEnd: false
        property int drawStartAtom: -1
        property bool drawDragged: false
        property bool skipNextClick: false
        property int chainStartAtom: -1
        property real chainStartX: 0
        property real chainStartY: 0
        property var chainSamples: []
        property real arrowStartX: 0
        property real arrowStartY: 0
        property var arrowPath: []
        property bool arrowDragging: false
        onPressed: function(mouse) {
            const point = root.modelPoint(mouse.x, mouse.y)
            dragStartX = mouse.x; dragStartY = mouse.y
            if (mouse.button === Qt.RightButton || activeTool === "erase") {
                // One eraser stroke is one undo step, however much it removes.
                erasing = true
                editHistory.beginGroup()
                root.eraseAt(point.x, point.y)
                eraseLastX = point.x; eraseLastY = point.y
                return
            }
            if (activeTool === "chain") {
                chainStartAtom = molecule.atomAt(point.x, point.y, 22 / root.zoom)
                const startAtom = root.atomById(chainStartAtom)
                if (startAtom) { chainStartX = startAtom.x; chainStartY = startAtom.y }
                else {
                    const snapped = root.placementPoint(point.x, point.y)
                    chainStartX = snapped.x; chainStartY = snapped.y
                }
                chainSamples = [point]
                skipNextClick = false
            }
            if (activeTool === "draw") {
                drawStartAtom = molecule.atomAt(point.x, point.y, 22 / root.zoom)
                drawDragged = false
                skipNextClick = false
            }
            if (activeTool === "arrow" && !awaitingArrowEnd) {
                arrowPath = [{x: point.x, y: point.y}]
                arrowDragging = false
                skipNextClick = false
            }
            if (activeTool === "select") {
                root.lassoPoints = [{ x: mouse.x, y: mouse.y }]
                root.boxSelectionCommitted = false
                drawing.requestPaint()
            }
            if (activeTool === "move") {
                dragAtom = molecule.atomAt(point.x, point.y, 20 / root.zoom)
                const atom = root.atomById(dragAtom)
                if (atom) {
                    if (root.selectedAtoms.indexOf(dragAtom) < 0) root.selectOnly(dragAtom)
                    atomStartX = atom.x; atomStartY = atom.y; modelStartX = point.x; modelStartY = point.y
                    dragOriginalPositions = ({})
                    for (let index = 0; index < root.selectedAtoms.length; ++index) {
                        const original = root.atomById(root.selectedAtoms[index])
                        if (original) dragOriginalPositions[original.id] = { x: original.x, y: original.y }
                    }
                } else {
                    dragReactionItem = reactionLayer.itemAt(point.x, point.y, 16 / root.zoom)
                    if (dragReactionItem >= 0) { modelStartX = point.x; modelStartY = point.y }
                }
            }
        }
        onPositionChanged: function(mouse) {
            if (erasing) {
                const point = root.modelPoint(mouse.x, mouse.y)
                root.eraseAlong(eraseLastX, eraseLastY, point.x, point.y)
                eraseLastX = point.x; eraseLastY = point.y
                return
            }
            if (pressed && activeTool === "chain" && chainSamples.length > 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                chainSamples.push(point)
                const params = root.chainParameters(chainStartAtom, chainStartX, chainStartY, chainSamples)
                if (params.count >= 1) {
                    root.chainPreviewPoints = molecule.chainPoints(chainStartAtom, chainStartX, chainStartY,
                                                                   params.angle, params.count, params.sign)
                    root.chainPreviewLabel = "C" + params.count
                } else {
                    root.chainPreviewPoints = []
                    root.chainPreviewLabel = ""
                }
                root.chainPreviewLabelX = point.x; root.chainPreviewLabelY = point.y
                drawing.requestPaint()
                return
            }
            if (pressed && activeTool === "draw" && drawStartAtom >= 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                if (Math.abs(mouse.x - dragStartX) > 12 || Math.abs(mouse.y - dragStartY) > 12) {
                    drawDragged = true
                    root.bondPreviewVisible = true
                    root.bondPreviewAtom = drawStartAtom
                    root.bondPreviewX = point.x
                    root.bondPreviewY = point.y
                    drawing.requestPaint()
                }
            }
            if (pressed && activeTool === "arrow" && arrowPath.length > 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                if (Math.abs(mouse.x - dragStartX) > 12 || Math.abs(mouse.y - dragStartY) > 12) arrowDragging = true
                if (arrowDragging) {
                    arrowPath.push({x: point.x, y: point.y})
                    root.arrowPreviewPath = arrowPath
                    drawing.requestPaint()
                }
            }
            if (pressed && activeTool === "pan") {
                root.panX += mouse.x - dragStartX; root.panY += mouse.y - dragStartY
                dragStartX = mouse.x; dragStartY = mouse.y; drawing.requestPaint()
            }
            if (pressed && activeTool === "select") {
                root.lassoPoints = root.lassoPoints.concat([{ x: mouse.x, y: mouse.y }])
                drawing.requestPaint()
            }
            if (pressed && activeTool === "move" && dragAtom >= 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                const dx = point.x - modelStartX, dy = point.y - modelStartY
                root.atomItems = root.atomItems.map(function(atom) {
                    if (root.selectedAtoms.indexOf(atom.id) >= 0) {
                        const original = dragOriginalPositions[atom.id]
                        return Object.assign({}, atom, {x: original.x + dx, y: original.y + dy})
                    }
                    return atom
                })
                drawing.requestPaint()
            }
        }
        onReleased: function(mouse) {
            if (erasing) {
                erasing = false
                editHistory.endGroup()
                skipNextClick = true
                root.selectedAtomChangedByCanvas(-1)
                return
            }
            if (activeTool === "chain" && chainSamples.length > 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                chainSamples.push(point)
                const params = root.chainParameters(chainStartAtom, chainStartX, chainStartY, chainSamples)
                if (params.count >= 1) {
                    if (molecule.addChain(chainStartAtom, chainStartX, chainStartY, params.angle, params.count, params.sign) >= 0)
                        status("Chain of " + params.count + " carbons added.")
                } else {
                    status("Drag to draw a chain.")
                }
                chainSamples = []
                chainStartAtom = -1
                root.chainPreviewPoints = []
                root.chainPreviewLabel = ""
                root.selectedAtomChangedByCanvas(-1)
                skipNextClick = true
                drawing.requestPaint()
                return
            }
            if (activeTool === "draw" && drawStartAtom >= 0 && drawDragged) {
                const point = root.modelPoint(mouse.x, mouse.y)
                const target = molecule.atomAt(point.x, point.y, 22 / root.zoom)
                if (target >= 0 && target !== drawStartAtom) {
                    molecule.addBond(drawStartAtom, target)
                } else {
                    const end = root.placementPoint(point.x, point.y)
                    molecule.extendFromAtom(drawStartAtom, end.x, end.y)
                }
                root.selectedAtomChangedByCanvas(-1)
                skipNextClick = true
            }
            if (activeTool === "arrow" && arrowDragging) {
                // Press-drag-release draws the arrow in one undo step; a short press
                // falls through to the tap-start / tap-end flow in onClicked.
                const point = root.modelPoint(mouse.x, mouse.y)
                const path = arrowPath.slice()
                path.push({x: point.x, y: point.y})
                if (reactionLayer.addArrowFromPath(root.arrowKind, path) >= 0) status("Reaction arrow added.")
                else status("Reaction arrows need a longer stroke.")
                skipNextClick = true
            }
            arrowPath = []
            arrowDragging = false
            root.arrowPreviewPath = []
            root.bondPreviewVisible = false
            root.bondPreviewAtom = -1
            drawing.requestPaint()
            if (activeTool === "select") {
                const loop = root.lassoPoints
                root.lassoPoints = []
                let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity
                for (let i = 0; i < loop.length; ++i) {
                    minX = Math.min(minX, loop[i].x); maxX = Math.max(maxX, loop[i].x)
                    minY = Math.min(minY, loop[i].y); maxY = Math.max(maxY, loop[i].y)
                }
                // A tiny loop is a tap, handled by onClicked.
                if (loop.length >= 3 && maxX - minX > 6 && maxY - minY > 6) {
                    const modelLoop = loop.map(function(point) { return root.modelPoint(point.x, point.y) })
                    root.selectedAtoms = molecule.atomsInPolygon(modelLoop)
                    root.selectedAtomChangedByCanvas(root.selectedAtoms.length ? root.selectedAtoms[root.selectedAtoms.length - 1] : -1)
                    root.boxSelectionCommitted = true
                }
                drawing.requestPaint()
            }
            if (activeTool === "move" && dragAtom >= 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                molecule.translateAtoms(root.selectedAtoms, point.x - modelStartX, point.y - modelStartY)
                selectedAtomChangedByCanvas(dragAtom)
            }
            if (activeTool === "move" && dragReactionItem >= 0) {
                const point = root.modelPoint(mouse.x, mouse.y)
                reactionLayer.translateItem(dragReactionItem, point.x - modelStartX, point.y - modelStartY)
            }
            dragAtom = -1
            dragReactionItem = -1
        }
        onCanceled: {
            if (root.lassoPoints.length) { root.lassoPoints = []; drawing.requestPaint() }
            if (erasing) { erasing = false; editHistory.endGroup() }
        }
        onClicked: function(mouse) {
            if (skipNextClick) {
                skipNextClick = false
                drawStartAtom = -1
                return
            }
            if (activeTool === "select" && root.boxSelectionCommitted) {
                root.boxSelectionCommitted = false
                return
            }
            const point = root.modelPoint(mouse.x, mouse.y)
            const hitAtom = molecule.atomAt(point.x, point.y, 28 / root.zoom)
            const hitBond = molecule.bondAt(point.x, point.y, 20 / root.zoom)
            const ringAtom = molecule.atomAt(point.x, point.y, 18 / root.zoom)
            if (activeTool === "draw") {
                if (hitAtom >= 0) {
                    let end = root.suggestedBondEnd(hitAtom)
                    if (root.gridSnapEnabled) end = root.snapToHexGrid(end.x, end.y)
                    if (molecule.extendFromAtom(hitAtom, end.x, end.y) >= 0)
                        status("Carbon extended from the tapped atom.")
                } else {
                    const placed = root.placementPoint(point.x, point.y)
                    molecule.addAtom(placed.x, placed.y)
                }
                root.selectedAtomChangedByCanvas(-1)
                return
            }
            if (activeTool === "arrow") {
                if (!awaitingArrowEnd) {
                    arrowStartX = point.x; arrowStartY = point.y; awaitingArrowEnd = true
                    status("Arrow start set; tap its endpoint.")
                } else {
                    if (reactionLayer.addArrowKind(root.arrowKind, arrowStartX, arrowStartY, point.x, point.y) >= 0) status("Reaction arrow added.")
                    else status("Reaction arrows need a longer stroke.")
                    awaitingArrowEnd = false
                }
                return
            }
            if (activeTool === "label") {
                if (reactionLayer.addLabelSnapped(root.labelText, point.x, point.y, 40 / root.zoom) >= 0) status("Text label added.")
                else status("Enter label text before placing it.")
                return
            }
            if (activeTool === "bond") {
                if (hitBond >= 0) molecule.cycleBondOrder(hitBond)
                else status("Tap an existing bond to change its order.")
                return
            }
            if (activeTool === "stereo") {
                if (hitBond >= 0) molecule.cycleBondStyle(hitBond)
                else status("Tap a bond to cycle its style (double bonds: centred or offset to either side).")
                return
            }
            if (activeTool === "select") {
                if (hitAtom >= 0) {
                    if (mouse.modifiers & Qt.ControlModifier) root.toggleSelection(hitAtom)
                    else root.selectOnly(hitAtom)
                } else if (!(mouse.modifiers & Qt.ControlModifier)) root.selectOnly(-1)
                return
            }
            if (activeTool === "move") {
                if (hitAtom >= 0) selectedAtomChangedByCanvas(hitAtom)
                return
            }
            if (activeTool === "ring") {
                let placed = false
                if (ringAtom >= 0)
                    placed = molecule.stampRingAtAtom(ringTemplate, ringAtom, point.x, point.y)
                else if (hitBond >= 0)
                    placed = molecule.stampRingAtBond(ringTemplate, hitBond, point.x, point.y)
                else if (selectedAtom >= 0)
                    placed = molecule.stampRingAtAtom(ringTemplate, selectedAtom, point.x, point.y)
                else
                    placed = molecule.stampRing(ringTemplate, point.x, point.y)
                if (placed) {
                    status("Ring attached.")
                    selectedAtomChangedByCanvas(-1)
                } else {
                    status("Ring could not attach there; try a carbon or the middle of a bond.")
                }
                return
            }
            // Pan (and any tool not handled above) does nothing on release. MouseArea
            // reports a click even after a drag, so falling through here used to drop
            // a stray carbon at the end of every pan gesture.
        }
    }

    WheelHandler {
        onWheel: function(event) {
            root.zoomAt(event.angleDelta.y > 0 ? 1.15 : 1 / 1.15, event.point.position.x, event.point.position.y)
            event.accepted = true
        }
    }

    // Two-finger pinch on desktop touchscreens and trackpads. The tablet build
    // receives single-pointer input only, so it relies on the zoom buttons.
    PinchHandler {
        id: pinch
        target: null
        property real startZoom: 1.0
        onActiveChanged: if (active) startZoom = root.zoom
        onActiveScaleChanged: if (active) root.zoomTo(startZoom * activeScale, centroid.position.x, centroid.position.y)
    }

    Connections { target: molecule; function onChanged() { root.refresh() } }
    Connections { target: reactionLayer; function onChanged() { root.refresh() } }
    Component.onCompleted: refresh()
}
