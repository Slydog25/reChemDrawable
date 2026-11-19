import QtQuick

// Small drawing of a template: {atoms: [{x, y, element}], bonds: [{a, b, order, style}]}.
Canvas {
    id: root
    property var data: ({})
    property real newmanRadius: 28
    onDataChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const atoms = data.atoms || [], bonds = data.bonds || []
        if (atoms.length === 0) return
        let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9
        for (const a of atoms) {
            minX = Math.min(minX, a.x); maxX = Math.max(maxX, a.x)
            minY = Math.min(minY, a.y); maxY = Math.max(maxY, a.y)
        }
        const margin = 12
        const scale = Math.min(0.7, (width - 2 * margin) / Math.max(maxX - minX, 1), (height - 2 * margin) / Math.max(maxY - minY, 1))
        const ox = width / 2 - (minX + maxX) / 2 * scale, oy = height / 2 - (minY + maxY) / 2 * scale
        const px = function(a) { return a.x * scale + ox }
        const py = function(a) { return a.y * scale + oy }
        ctx.strokeStyle = "#111111"
        ctx.fillStyle = "#111111"
        ctx.lineWidth = 1.6
        ctx.lineCap = "round"
        for (const b of bonds) {
            const a = atoms[b.a], c = atoms[b.b]
            const x1 = px(a), y1 = py(a), x2 = px(c), y2 = py(c)
            const dx = x2 - x1, dy = y2 - y1, len = Math.sqrt(dx * dx + dy * dy)
            const nx = len > 0 ? -dy / len : 0, ny = len > 0 ? dx / len : 0
            if (b.style === "newman") {
                ctx.beginPath(); ctx.arc((x1 + x2) / 2, (y1 + y2) / 2, newmanRadius * scale, 0, 2 * Math.PI); ctx.stroke()
                continue
            }
            if (b.style === "wedge") {
                ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2 + nx * 3, y2 + ny * 3); ctx.lineTo(x2 - nx * 3, y2 - ny * 3)
                ctx.closePath(); ctx.fill(); continue
            }
            if (b.style === "dash") {
                for (let i = 1; i < 6; ++i) {
                    const t = i / 6, w = 0.5 + t * 3
                    ctx.beginPath(); ctx.moveTo(x1 + dx * t + nx * w, y1 + dy * t + ny * w)
                    ctx.lineTo(x1 + dx * t - nx * w, y1 + dy * t - ny * w); ctx.stroke()
                }
                continue
            }
            let sx = x1, sy = y1
            if (b.style === "newmanback" && len > 0) {
                const inset = Math.min(newmanRadius * scale, len) / len
                sx = x1 + dx * inset; sy = y1 + dy * inset
            }
            ctx.lineWidth = b.style === "bold" ? 4 : 1.6
            const offsets = b.order === 2 ? [-2, 2] : b.order >= 3 ? [-3, 0, 3] : [0]
            for (const o of offsets) {
                ctx.beginPath(); ctx.moveTo(sx + nx * o, sy + ny * o); ctx.lineTo(x2 + nx * o, y2 + ny * o); ctx.stroke()
            }
            ctx.lineWidth = 1.6
        }
        ctx.font = "bold 13px sans-serif"
        ctx.textAlign = "center"
        ctx.textBaseline = "middle"
        for (const a of atoms) {
            if (a.element === "C") continue
            const x = px(a), y = py(a)
            ctx.fillStyle = "#ffffff"
            ctx.fillRect(x - 7, y - 8, 14, 16)
            ctx.fillStyle = "#111111"
            ctx.fillText(a.element, x, y)
        }
    }
}
