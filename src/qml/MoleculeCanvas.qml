import QtQuick

Rectangle {
    id: root
    property string activeTool: "draw"
    property string ringTemplate: "cyclohexane"
    property int selectedAtom: -1
    property var selectedAtoms: []
    property bool selectionBoxVisible: false
    property real selectionBoxX: 0
    property real selectionBoxY: 0
    property real selectionBoxWidth: 0
    property real selectionBoxHeight: 0
    property bool boxSelectionCommitted: false
    property real zoom: 1.0
    property real panX: 0
    property real panY: 0
    property bool gridSnapEnabled: false
    property var atomItems: []
    property var bondItems: []
    property var reactionItems: []
    property bool bondPreviewVisible: false
    property int bondPreviewAtom: -1
    property real bondPreviewX: 0
    property real bondPreviewY: 0
    property string labelText: "conditions"
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
            if (root.selectionBoxVisible) {
                ctx.strokeStyle = "#756100"
                ctx.lineWidth = 1
                ctx.setLineDash([4, 3])
                ctx.strokeRect(root.selectionBoxX, root.selectionBoxY, root.selectionBoxWidth, root.selectionBoxHeight)
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
            ctx.lineWidth = 3
            for (let i = 0; i < reactionItems.length; ++i) {
                const item = reactionItems[i]
                if (item.type === "arrow") {
                    const dx = item.x2 - item.x1, dy = item.y2 - item.y1
                    const length = Math.sqrt(dx * dx + dy * dy)
                    if (length < 0.01) continue
                    const ux = dx / length, uy = dy / length, nx = -uy, ny = ux
                    ctx.beginPath(); ctx.moveTo(item.x1, item.y1); ctx.lineTo(item.x2, item.y2); ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(item.x2 - ux * 13 + nx * 7, item.y2 - uy * 13 + ny * 7)
                    ctx.lineTo(item.x2, item.y2)
                    ctx.lineTo(item.x2 - ux * 13 - nx * 7, item.y2 - uy * 13 - ny * 7)
                    ctx.stroke()
                } else if (item.type === "label") {
                    ctx.font = "18px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    ctx.fillText(item.text, item.x1, item.y1)
                }
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
                    ctx.lineWidth = 3
                }
            }
            ctx.font = "20px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            for (let i = 0; i < atomItems.length; ++i) {
                const atom = atomItems[i]
                const charge = atom.charge === 1 ? "+" : atom.charge === -1 ? "−" : atom.charge ? Math.abs(atom.charge) + (atom.charge > 0 ? "+" : "−") : ""
                const label = (atom.isotope > 0 ? atom.isotope : "") + atom.element + charge
                if (atom.element === "C" && atom.charge === 0 && atom.isotope === 0) {
                    ctx.fillStyle = root.atomColor(atom.element)
                    ctx.beginPath(); ctx.arc(atom.x, atom.y, 4, 0, 2 * Math.PI); ctx.fill()
                } else {
                    const metrics = ctx.measureText(label)
                    ctx.fillStyle = "white"
                    ctx.fillRect(atom.x - metrics.width / 2 - 4, atom.y - 14, metrics.width + 8, 28)
                    ctx.fillStyle = root.atomColor(atom.element)
                    ctx.fillText(label, atom.x, atom.y)
                }
                if (selectedAtoms.indexOf(atom.id) >= 0 || atom.id === selectedAtom) {
                    ctx.strokeStyle = "#756100"
                    ctx.lineWidth = 2
                    ctx.beginPath(); ctx.arc(atom.x, atom.y, 16, 0, 2 * Math.PI); ctx.stroke()
                    ctx.strokeStyle = "#121212"; ctx.lineWidth = 3
                }
            }
            ctx.restore()
        }
    }

    MouseArea {
        id: canvasInput
        objectName: "canvasInput"
        anchors.fill: parent
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
        property real arrowStartX: 0
        property real arrowStartY: 0
        onPressed: function(mouse) {
            const point = root.modelPoint(mouse.x, mouse.y)
            dragStartX = mouse.x; dragStartY = mouse.y
            if (activeTool === "draw") {
                drawStartAtom = molecule.atomAt(point.x, point.y, 22 / root.zoom)
                drawDragged = false
                skipNextClick = false
            }
            if (activeTool === "select") {
                root.selectionBoxVisible = true
                root.selectionBoxX = mouse.x; root.selectionBoxY = mouse.y
                root.selectionBoxWidth = 0; root.selectionBoxHeight = 0
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
            if (pressed && activeTool === "pan") {
                root.panX += mouse.x - dragStartX; root.panY += mouse.y - dragStartY
                dragStartX = mouse.x; dragStartY = mouse.y; drawing.requestPaint()
            }
            if (pressed && activeTool === "select") {
                root.selectionBoxX = Math.min(dragStartX, mouse.x)
                root.selectionBoxY = Math.min(dragStartY, mouse.y)
                root.selectionBoxWidth = Math.abs(mouse.x - dragStartX)
                root.selectionBoxHeight = Math.abs(mouse.y - dragStartY)
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
            root.bondPreviewVisible = false
            root.bondPreviewAtom = -1
            drawing.requestPaint()
            if (activeTool === "select" && root.selectionBoxWidth > 6 && root.selectionBoxHeight > 6) {
                const first = root.modelPoint(dragStartX, dragStartY)
                const last = root.modelPoint(mouse.x, mouse.y)
                const minX = Math.min(first.x, last.x), maxX = Math.max(first.x, last.x)
                const minY = Math.min(first.y, last.y), maxY = Math.max(first.y, last.y)
                root.selectedAtoms = root.atomItems.filter(function(atom) {
                    return atom.x >= minX && atom.x <= maxX && atom.y >= minY && atom.y <= maxY
                }).map(function(atom) { return atom.id })
                root.selectedAtomChangedByCanvas(root.selectedAtoms.length ? root.selectedAtoms[root.selectedAtoms.length - 1] : -1)
                root.boxSelectionCommitted = true
            }
            if (activeTool === "select") {
                root.selectionBoxVisible = false
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
                    if (reactionLayer.addArrow(arrowStartX, arrowStartY, point.x, point.y) >= 0) status("Reaction arrow added.")
                    else status("Reaction arrows need a longer stroke.")
                    awaitingArrowEnd = false
                }
                return
            }
            if (activeTool === "label") {
                if (reactionLayer.addLabel(root.labelText, point.x, point.y) >= 0) status("Text label added.")
                else status("Enter label text before placing it.")
                return
            }
            if (activeTool === "erase") {
                const reactionItem = reactionLayer.itemAt(point.x, point.y, 16 / root.zoom)
                if (reactionItem >= 0) reactionLayer.removeItem(reactionItem)
                else if (hitAtom >= 0) molecule.removeAtom(hitAtom)
                else if (hitBond >= 0) molecule.removeBond(hitBond)
                selectedAtomChangedByCanvas(-1)
                return
            }
            if (activeTool === "bond") {
                if (hitBond >= 0) molecule.cycleBondOrder(hitBond)
                else status("Tap an existing bond to change its order.")
                return
            }
            if (activeTool === "stereo") {
                if (hitBond >= 0) molecule.cycleBondStyle(hitBond)
                else status("Tap a bond to cycle plain, wedge, dash, wavy, and aromatic styles.")
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
