import QtQuick

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
        const box = function(x, y, w, h) {
            ctx.strokeRect(x, y, w, h)
        }
        const arrow = function(x1, y1, x2, y2) {
            line(x1, y1, x2, y2)
            const angle = Math.atan2(y2 - y1, x2 - x1)
            line(x2, y2, x2 - 8 * Math.cos(angle - 0.55), y2 - 8 * Math.sin(angle - 0.55))
            line(x2, y2, x2 - 8 * Math.cos(angle + 0.55), y2 - 8 * Math.sin(angle + 0.55))
        }
        const hexagon = function() {
            ctx.beginPath(); ctx.moveTo(22, 5); ctx.lineTo(36, 13); ctx.lineTo(36, 30)
            ctx.lineTo(22, 39); ctx.lineTo(8, 30); ctx.lineTo(8, 13); ctx.closePath(); ctx.stroke()
        }
        switch (name) {
        case "draw": line(12, 34, 31, 15); line(28, 12, 34, 18); line(10, 36, 16, 34); break
        case "select": box(8, 8, 27, 27); line(8, 8, 15, 8); line(8, 8, 8, 15); line(35, 35, 28, 35); line(35, 35, 35, 28); break
        case "move": arrow(22, 22, 22, 5); arrow(22, 22, 22, 39); arrow(22, 22, 5, 22); arrow(22, 22, 39, 22); break
        case "pan": ctx.beginPath(); ctx.arc(22, 24, 10, 0, Math.PI * 2); ctx.stroke(); line(16, 12, 16, 28); line(22, 10, 22, 27); line(28, 12, 28, 28); break
        case "bond": line(7, 15, 37, 15); line(7, 23, 37, 23); line(7, 31, 37, 31); break
        case "style": ctx.beginPath(); ctx.moveTo(8, 35); ctx.lineTo(22, 7); ctx.lineTo(36, 35); ctx.closePath(); ctx.fill(); break
        case "ring": hexagon(); break
        case "arrow": arrow(6, 22, 38, 22); break
        case "text": line(9, 8, 35, 8); line(22, 8, 22, 36); break
        case "erase": ctx.save(); ctx.translate(22, 23); ctx.rotate(-0.55); ctx.strokeRect(-12, -8, 24, 16); ctx.restore(); line(9, 36, 35, 36); break
        case "touch": ctx.beginPath(); ctx.arc(22, 12, 5, Math.PI, 0); ctx.lineTo(27, 30); ctx.quadraticCurveTo(27, 38, 20, 38); ctx.lineTo(14, 38); ctx.quadraticCurveTo(9, 36, 10, 31); ctx.lineTo(10, 22); ctx.quadraticCurveTo(10, 18, 14, 18); ctx.lineTo(14, 29); line(18, 10, 18, 28); line(22, 7, 22, 28); ctx.stroke(); break
        case "grid": for (let row = 0; row < 3; ++row) { for (let column = 0; column < 3; ++column) { const x = 12 + column * 10 + (row % 2 ? 5 : 0); const y = 12 + row * 10; ctx.beginPath(); ctx.arc(x, y, 2, 0, Math.PI * 2); ctx.fill() } } line(12, 12, 22, 12); line(22, 12, 17, 22); line(17, 22, 27, 22); line(27, 22, 22, 32); break
        case "undo": ctx.beginPath(); ctx.moveTo(36, 29); ctx.quadraticCurveTo(28, 7, 13, 14); ctx.lineTo(8, 19); ctx.stroke(); line(8, 19, 17, 19); line(8, 19, 12, 10); break
        case "redo": ctx.beginPath(); ctx.moveTo(8, 29); ctx.quadraticCurveTo(16, 7, 31, 14); ctx.lineTo(36, 19); ctx.stroke(); line(36, 19, 27, 19); line(36, 19, 32, 10); break
        case "load": box(7, 14, 30, 23); line(7, 14, 19, 14); line(11, 10, 22, 10); arrow(22, 5, 22, 27); break
        case "save": box(8, 6, 28, 32); box(14, 7, 15, 10); box(14, 25, 16, 12); break
        case "scheme": hexagon(); arrow(6, 40, 38, 40); break
        case "svg": box(6, 7, 32, 30); ctx.font = "bold 13px sans-serif"; ctx.fillText("SVG", 9, 27); break
        case "clean": line(10, 34, 30, 14); line(24, 10, 34, 20); line(8, 37, 18, 37); break
        case "rotate": ctx.beginPath(); ctx.arc(22, 22, 14, -1.15, 4.2); ctx.stroke(); arrow(31, 8, 38, 11); break
        case "flipH": line(22, 5, 22, 39); arrow(19, 22, 5, 22); arrow(25, 22, 39, 22); break
        case "flipV": line(5, 22, 39, 22); arrow(22, 19, 22, 5); arrow(22, 25, 22, 39); break
        case "alignH": line(5, 22, 39, 22); box(10, 9, 8, 7); box(25, 28, 10, 7); break
        case "alignV": line(22, 5, 22, 39); box(8, 10, 7, 8); box(28, 25, 7, 10); break
        case "copy": box(8, 7, 23, 27); box(14, 13, 23, 27); break
        case "paste": box(10, 9, 24, 30); box(16, 5, 12, 8); line(15, 24, 29, 24); line(15, 30, 27, 30); break
        case "delete": box(11, 13, 22, 25); line(8, 12, 36, 12); line(17, 7, 27, 7); line(18, 18, 18, 32); line(26, 18, 26, 32); break
        case "clear": line(9, 9, 35, 35); line(35, 9, 9, 35); break
        }
    }

    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
}
