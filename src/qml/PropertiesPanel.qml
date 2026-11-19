import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Molecular property panel: values from Molecule::propertyReport() for the whole
// drawing, or for the selected connected components. Holds no molecule model;
// it re-reads the report whenever the drawing or the selection changes.
Popup {
    id: panel
    objectName: "propertiesPanel"

    // Atom ids selected on the canvas (empty = whole drawing).
    property var selectedAtoms: []
    property var report: ({})

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    modal: true
    padding: 20
    width: 940

    function refresh() {
        if (visible) report = molecule.propertyReport(selectedAtoms)
    }
    onOpened: refresh()
    onSelectedAtomsChanged: refresh()
    Connections {
        target: molecule
        function onChanged() { panel.refresh() }
    }

    function fixed(value, digits) { return Number(value).toFixed(digits) }

    component Row2: RowLayout {
        property string name: ""
        property string value: ""
        property string objectId: ""
        Layout.fillWidth: true
        Label { text: parent.name; font.pixelSize: 21; color: "#444"; Layout.preferredWidth: 300 }
        Label { objectName: parent.objectId; text: parent.value; font.pixelSize: 21; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Properties"; font.pixelSize: 30; font.bold: true }
            Label {
                objectName: "propertiesScope"
                Layout.leftMargin: 14
                text: report.selectionOnly ? "selection (" + report.atomCount + " atoms)" : "whole drawing"
                font.pixelSize: 20
                color: "#5a5a55"
            }
            Item { Layout.fillWidth: true }
            Button { objectName: "propertiesClose"; text: "Close"; font.pixelSize: 20; Layout.preferredHeight: 56; onClicked: panel.close() }
        }

        Label {
            visible: !report.atomCount
            text: "Draw a structure to see its properties."
            font.pixelSize: 21
        }

        ColumnLayout {
            visible: report.atomCount > 0
            Layout.fillWidth: true
            spacing: 6
            Row2 { name: "Formula"; objectId: "propFormula"; value: report.formula || "" }
            Row2 { name: "Molecular weight"; value: fixed(report.molecularWeight, 2) + " g/mol" }
            Row2 { name: "Exact mass"; value: fixed(report.exactMass, 4) }
            Row2 { name: "Degrees of unsaturation"; objectId: "propDou"; value: fixed(report.degreesOfUnsaturation, report.degreesOfUnsaturation % 1 === 0 ? 0 : 1) }
            Row2 { name: "H-bond donors / acceptors"; objectId: "propHbond"; value: report.hydrogenBondDonors + " / " + report.hydrogenBondAcceptors }
            Row2 {
                name: "Lipinski rule of five"
                value: (report.ruleOfFiveViolations === 0 ? "no violations" : report.ruleOfFiveViolations + " violation(s)") + " (no logP)"
            }
            Row2 {
                name: "TPSA (N, O)"
                objectId: "propTpsa"
                value: fixed(report.tpsa, 1) + " Å²" + (report.tpsaWithSulfurPhosphorus > report.tpsa + 0.005 ? "   (with S, P: " + fixed(report.tpsaWithSulfurPhosphorus, 1) + ")" : "")
            }
            Label {
                visible: (report.unknownElements || []).length > 0
                text: "No mass data for " + (report.unknownElements || []).join(", ") + "; those atoms are left out."
                font.pixelSize: 18; color: "#6a5a00"
            }

            Label { text: "Elemental analysis"; font.pixelSize: 22; font.bold: true; Layout.topMargin: 8 }
            Flow {
                Layout.fillWidth: true
                spacing: 24
                Repeater {
                    model: report.elements || []
                    delegate: Label { text: modelData.symbol + " " + fixed(modelData.percent, 2) + " %"; font.pixelSize: 21 }
                }
            }

            Label { text: "Mass spectrum"; font.pixelSize: 22; font.bold: true; Layout.topMargin: 8 }
            Flow {
                Layout.fillWidth: true
                spacing: 24
                Repeater {
                    model: report.adducts || []
                    delegate: Label { text: modelData.label + " " + fixed(modelData.mz, 4); font.pixelSize: 21 }
                }
            }
            Label {
                visible: (report.pattern || []).length === 0
                text: "Isotope pattern unavailable."
                font.pixelSize: 19
            }
            Canvas {
                id: chart
                objectName: "isotopeChart"
                visible: (report.pattern || []).length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                contextType: "2d"
                property var pattern: report.pattern || []
                onPatternChanged: requestPaint()
                onWidthChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const peaks = pattern
                    if (!peaks.length) return
                    const left = 46, right = width - 8, top = 18, bottom = height - 54
                    ctx.strokeStyle = "#171717"; ctx.fillStyle = "#171717"; ctx.lineWidth = 2
                    ctx.font = "16px sans-serif"
                    ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, bottom); ctx.lineTo(right, bottom); ctx.stroke()
                    ctx.textAlign = "right"; ctx.textBaseline = "middle"
                    for (let t = 0; t <= 100; t += 50) {
                        const y = bottom - (bottom - top) * t / 100
                        ctx.fillText(t, left - 6, y)
                    }
                    // Show at most 16 peaks, starting at the first one.
                    const shown = peaks.slice(0, 16)
                    const slot = (right - left) / shown.length
                    const barWidth = Math.min(48, slot * 0.6)
                    ctx.textAlign = "center"; ctx.textBaseline = "top"
                    for (let i = 0; i < shown.length; ++i) {
                        const cx = left + slot * (i + 0.5)
                        const h = (bottom - top) * shown[i].relative / 100
                        ctx.fillRect(cx - barWidth / 2, bottom - h, barWidth, Math.max(h, 1))
                        ctx.fillText(shown[i].label, cx, bottom + 4)
                        ctx.fillText(Number(shown[i].mass).toFixed(2), cx, bottom + 24)
                        if (shown[i].relative >= 1) {
                            ctx.textBaseline = "bottom"
                            ctx.fillText(Number(shown[i].relative).toFixed(1), cx, bottom - h - 2)
                            ctx.textBaseline = "top"
                        }
                    }
                }
            }
        }
    }
}
