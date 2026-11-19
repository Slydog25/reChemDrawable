import QtQuick

// Toolbar icons, drawn as monochrome line art on a 44x44 canvas so they stay
// crisp on e-ink and invert cleanly when a button is selected.
Canvas {
    id: icon
    property string name: ""
    property color ink: "#111111"
    width: 44
    height: 44
    contextType: "2d"
    antialiasing: true

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = ink
        ctx.fillStyle = ink
        ctx.lineWidth = 3
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        const line = function(x1, y1, x2, y2) {
            ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke()
        }
        const path = function(points, closed) {
            ctx.beginPath()
            ctx.moveTo(points[0][0], points[0][1])
            for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1])
            if (closed) ctx.closePath()
            ctx.stroke()
        }
        const solid = function(points) {
            ctx.beginPath()
            ctx.moveTo(points[0][0], points[0][1])
            for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1])
            ctx.closePath()
            ctx.fill()
        }
        const dot = function(x, y, r) {
            ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill()
        }
        const box = function(x, y, w, h) { ctx.strokeRect(x, y, w, h) }
        // Filled arrowhead with its tip at (x, y), pointing along `angle`.
        const head = function(x, y, angle, size) {
            const s = size || 8
            solid([[x, y],
                   [x - s * Math.cos(angle - 0.5), y - s * Math.sin(angle - 0.5)],
                   [x - s * Math.cos(angle + 0.5), y - s * Math.sin(angle + 0.5)]])
        }
        const arrow = function(x1, y1, x2, y2) {
            const angle = Math.atan2(y2 - y1, x2 - x1)
            line(x1, y1, x2 - 5 * Math.cos(angle), y2 - 5 * Math.sin(angle))
            head(x2, y2, angle)
        }
        // Curved arrow along a circle, from `start` to `end` radians.
        const arcArrow = function(cx, cy, r, start, end, anticlockwise) {
            ctx.beginPath(); ctx.arc(cx, cy, r, start, end, anticlockwise); ctx.stroke()
            const tangent = end + (anticlockwise ? -Math.PI / 2 : Math.PI / 2)
            head(cx + r * Math.cos(end) + 2 * Math.cos(tangent), cy + r * Math.sin(end) + 2 * Math.sin(tangent), tangent, 9)
        }
        const chev = function(x, y, angle, side, size) {
            const s = size || 10, ca = Math.cos(angle), sa = Math.sin(angle)
            ctx.beginPath()
            if (side >= 0) { ctx.moveTo(x - s * ca + s * 0.55 * -sa, y - s * sa + s * 0.55 * ca); ctx.lineTo(x, y) }
            else ctx.moveTo(x, y)
            if (side <= 0) ctx.lineTo(x - s * ca - s * 0.55 * -sa, y - s * sa - s * 0.55 * ca)
            ctx.stroke()
        }
        const hexagon = function(cx, cy, r) {
            const points = []
            for (let i = 0; i < 6; ++i) points.push([cx + r * Math.cos(Math.PI / 6 + i * Math.PI / 3), cy + r * Math.sin(Math.PI / 6 + i * Math.PI / 3)])
            path(points, true)
        }
        // Page with a folded corner, optionally labelled (export formats).
        const page = function(label) {
            path([[9, 4], [27, 4], [36, 13], [36, 40], [9, 40]], true)
            path([[27, 4], [27, 13], [36, 13]], false)
            if (label) {
                ctx.font = "bold 9px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText(label, 22.5, 28)
            }
        }
        const sparkle = function(x, y, r) {
            line(x - r, y, x + r, y); line(x, y - r, x, y + r)
        }
        const dashed = function(x1, y1, x2, y2) {
            ctx.setLineDash([3, 3]); line(x1, y1, x2, y2); ctx.setLineDash([])
        }

        switch (name) {
        // Tools
        case "draw": // skeletal zig-zag chain
            path([[4, 30], [13, 21], [22, 30], [31, 21], [40, 30]], false)
            dot(4, 30, 2.5); dot(40, 30, 2.5)
            break
        case "select": // dashed marquee with a pointer
            ctx.lineWidth = 2.5; ctx.setLineDash([4, 3]); box(5, 5, 28, 24); ctx.setLineDash([]); ctx.lineWidth = 3
            solid([[22, 18], [22, 40], [27, 35], [31, 42], [35, 40], [31, 33], [38, 33]])
            break
        case "move":
            arrow(22, 22, 22, 4); arrow(22, 22, 22, 40); arrow(22, 22, 4, 22); arrow(22, 22, 40, 22)
            break
        case "pan": // open hand
            ctx.lineWidth = 5.5
            line(14, 24, 14, 12); line(20, 22, 20, 8); line(26, 22, 26, 9); line(32, 24, 32, 14)
            line(8, 24, 12, 31)
            ctx.lineWidth = 3
            solid([[11, 22], [35, 22], [35, 30], [31, 39], [16, 39], [11, 31]])
            break
        case "bond": // a double bond between two atoms
            dot(6, 22, 3.5); dot(38, 22, 3.5)
            line(12, 17, 32, 17); line(12, 27, 32, 27)
            break
        case "style": // solid wedge and hashed wedge
            solid([[5, 38], [15, 7], [22, 10]])
            ctx.lineWidth = 2
            for (let t = 0.15; t <= 1.0; t += 0.17) {
                const x = 24 + (35 - 24) * t, y = 38 + (8 - 38) * t, half = 1 + 4.5 * t
                line(x - half * 0.94, y - half * 0.34, x + half * 0.94, y + half * 0.34)
            }
            ctx.lineWidth = 3
            break
        case "ring": hexagon(22, 22, 16); break
        case "chain": // zig-zag with atom dots
            path([[4, 30], [14, 14], [24, 30], [34, 14], [40, 22]])
            dot(4, 30, 3); dot(24, 30, 3); dot(34, 14, 3)
            break
        case "arrow": arrow(4, 22, 40, 22); break
        // Reaction arrow kinds (Arrows row). chev: open barbs at (x, y) along `angle`;
        // side 0 both barbs, +1/-1 one barb (half head).
        case "akForward": line(4, 22, 40, 22); chev(40, 22, 0, 0); break
        case "akEquilibrium":
            line(4, 15, 40, 15); chev(40, 15, 0, -1)
            line(40, 29, 4, 29); chev(4, 29, Math.PI, -1)
            break
        case "akRetro":
            line(4, 17, 28, 17); line(4, 27, 28, 27); chev(40, 22, 0, 0, 14)
            break
        case "akResonance": line(8, 22, 36, 22); chev(40, 22, 0, 0); chev(4, 22, Math.PI, 0); break
        case "akDashed": line(4, 22, 11, 22); line(16, 22, 23, 22); line(28, 22, 34, 22); chev(40, 22, 0, 0); break
        case "akNoReaction": line(4, 22, 40, 22); chev(40, 22, 0, 0); line(16, 11, 28, 33); line(28, 11, 16, 33); break
        case "akCurved":
            ctx.beginPath(); ctx.moveTo(6, 36); ctx.quadraticCurveTo(22, -4, 38, 32); ctx.stroke()
            chev(38, 32, Math.atan2(32 + 4, 38 - 22), 0, 12)
            break
        case "akFishhook":
            ctx.beginPath(); ctx.moveTo(6, 36); ctx.quadraticCurveTo(22, -4, 38, 32); ctx.stroke()
            chev(38, 32, Math.atan2(32 + 4, 38 - 22), 1, 12)
            break
        case "text": // serif T
            line(9, 8, 35, 8); line(22, 8, 22, 37); line(9, 8, 9, 13); line(35, 8, 35, 13); line(16, 37, 28, 37)
            break
        case "erase": // eraser block with its rubber end filled
            ctx.save(); ctx.translate(22, 22); ctx.rotate(-0.7)
            ctx.strokeRect(-15, -8, 30, 16); ctx.fillRect(-15, -8, 11, 16)
            ctx.restore()
            break

        // History and view
        case "undo": arcArrow(22, 27, 14, 0.35, Math.PI + 0.2, true); break
        case "redo": arcArrow(22, 27, 14, Math.PI - 0.35, -0.2, false); break
        case "grid": // the snap lattice (points at +-30 and 90 degrees) with a chain on it
            for (let column = 0; column < 5; ++column) {
                for (let row = 0; row < 4; ++row) {
                    const y = 7 + row * 10 + (column % 2 ? 5 : 0)
                    if (y <= 40) dot(6 + column * 8, y, 1.6)
                }
            }
            ctx.lineWidth = 2.5
            path([[6, 27], [14, 22], [22, 27], [30, 22], [38, 27]], false)
            ctx.lineWidth = 3
            break
        case "zoomIn":
            ctx.beginPath(); ctx.arc(18, 18, 12, 0, Math.PI * 2); ctx.stroke()
            line(27, 27, 39, 39); line(12, 18, 24, 18); line(18, 12, 18, 24)
            break
        case "zoomOut":
            ctx.beginPath(); ctx.arc(18, 18, 12, 0, Math.PI * 2); ctx.stroke()
            line(27, 27, 39, 39); line(12, 18, 24, 18)
            break
        case "fit":
            path([[5, 14], [5, 5], [14, 5]], false); path([[30, 5], [39, 5], [39, 14]], false)
            path([[39, 30], [39, 39], [30, 39]], false); path([[14, 39], [5, 39], [5, 30]], false)
            hexagon(22, 22, 9)
            break

        // Files
        case "new": page(""); line(22.5, 20, 22.5, 34); line(15.5, 27, 29.5, 27); break
        case "load": // open folder
            path([[4, 11], [15, 11], [19, 15], [36, 15], [36, 20]], false)
            path([[4, 11], [4, 37], [34, 37], [40, 20], [10, 20], [4, 37]], false)
            break
        case "save": // floppy disk
            path([[6, 5], [32, 5], [39, 12], [39, 39], [6, 39]], true)
            box(13, 5, 15, 10); box(12, 24, 21, 15)
            break
        case "mol": page("MOL"); break
        case "svg": page("SVG"); break
        case "smiles": page("SMI"); break
        case "export": page("EXP"); break

        // Editing
        case "stereo": // stereocentre: one wedge, one hashed bond, two plain bonds
            line(22, 24, 9, 38); line(22, 24, 35, 38)
            solid([[22, 24], [31, 5], [38, 11]])
            for (let k = 0; k < 4; ++k) {
                const t = 0.3 + k * 0.22, w = 1.5 + k * 1.3
                const cx = 22 - 16 * t, cy = 24 - 16 * t
                line(cx - w * 0.7, cy + w * 0.7, cx + w * 0.7, cy - w * 0.7)
            }
            break
        case "clean": // magic wand
            ctx.lineWidth = 4; line(7, 38, 26, 19); ctx.lineWidth = 3
            sparkle(32, 12, 5); sparkle(19, 8, 3); sparkle(37, 25, 3)
            break
        case "rotate": arcArrow(22, 22, 14, Math.PI * 0.6, Math.PI * 0.2 + Math.PI * 2, false); break
        case "flipH": // mirror across a vertical axis
            dashed(22, 4, 22, 40)
            solid([[18, 10], [18, 33], [5, 33]]); path([[26, 10], [26, 33], [39, 33]], true)
            break
        case "flipV": // mirror across a horizontal axis
            dashed(4, 22, 40, 22)
            solid([[10, 18], [33, 18], [33, 5]]); path([[10, 26], [33, 26], [33, 39]], true)
            break
        case "alignH": // shapes centred on one horizontal line
            line(4, 22, 40, 22); box(8, 15, 11, 14); box(25, 10, 11, 24)
            break
        case "alignV": // shapes centred on one vertical line
            line(22, 4, 22, 40); box(15, 8, 14, 11); box(10, 25, 24, 11)
            break
        case "copy": box(6, 5, 22, 26); box(15, 13, 22, 26); break
        case "paste":
            box(9, 8, 26, 32); box(16, 4, 12, 8); line(15, 23, 29, 23); line(15, 30, 26, 30)
            break
        case "duplicate": // a fragment and its copy, offset down-right with a plus
            box(4, 4, 18, 18); line(9, 13, 17, 13); line(13, 9, 13, 17)
            box(23, 23, 17, 17)
            break
        case "bold": // a thick bond beside a thin one
            line(8, 12, 36, 12); ctx.lineWidth = 8; line(8, 30, 36, 30); ctx.lineWidth = 3
            break
        case "delete": // trash can
            line(7, 11, 37, 11); line(17, 6, 27, 6)
            path([[10, 11], [12, 39], [32, 39], [34, 11]], false); line(18, 17, 18, 33); line(26, 17, 26, 33)
            break
        case "exit": // leave through a door
            path([[24, 13], [24, 5], [6, 5], [6, 39], [24, 39], [24, 31]], false)
            arrow(14, 22, 40, 22)
            break
        case "clear": // empty the canvas
            box(5, 6, 34, 32); line(14, 14, 30, 30); line(30, 14, 14, 30)
            break
        }
    }

    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
}
