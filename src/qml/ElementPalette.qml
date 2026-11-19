import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flow {
    property int selectedAtom: -1
    signal elementSelected(string element)
    signal chargeRequested(int charge)
    signal groupRequested(string group)
    signal lonePairRequested()
    signal radicalRequested()
    signal circledChargesToggled(bool enabled)
    signal abbreviationRequested(string name)
    property bool circledCharges: false
    spacing: 8
    Label { text: "Atoms"; font.pixelSize: 19; font.bold: true; height: 58; verticalAlignment: Text.AlignVCenter }
    Repeater {
        model: ["C", "O", "N", "S", "P", "F", "Cl", "Br", "I", "B", "Si", "Se", "Li", "Na", "K", "Mg"]
        delegate: Button {
            text: modelData
            width: modelData.length > 1 ? 66 : 56
            height: 58
            font.pixelSize: 24
            enabled: selectedAtom >= 0
            onClicked: elementSelected(modelData)
        }
    }
    Button { text: "−"; width: 56; height: 58; font.pixelSize: 24; enabled: selectedAtom >= 0; onClicked: chargeRequested(-1) }
    Button { text: "0"; width: 56; height: 58; font.pixelSize: 24; enabled: selectedAtom >= 0; onClicked: chargeRequested(0) }
    Button { text: "+"; width: 56; height: 58; font.pixelSize: 24; enabled: selectedAtom >= 0; onClicked: chargeRequested(1) }
    Button { text: "OH"; width: 66; height: 58; font.pixelSize: 21; enabled: selectedAtom >= 0; onClicked: groupRequested("OH") }
    Button { text: "NH₂"; width: 76; height: 58; font.pixelSize: 19; enabled: selectedAtom >= 0; onClicked: groupRequested("NH2") }
    Button { text: "Me"; width: 66; height: 58; font.pixelSize: 20; enabled: selectedAtom >= 0; onClicked: groupRequested("Me") }
    Button { text: "COOH"; width: 88; height: 58; font.pixelSize: 17; enabled: selectedAtom >= 0; onClicked: groupRequested("COOH") }
    Button { text: "CN"; width: 66; height: 58; font.pixelSize: 20; enabled: selectedAtom >= 0; onClicked: groupRequested("CN") }
    Button { text: "NO₂"; width: 76; height: 58; font.pixelSize: 19; enabled: selectedAtom >= 0; onClicked: groupRequested("NO2") }
    Button { text: "tBu"; width: 76; height: 58; font.pixelSize: 19; enabled: selectedAtom >= 0; onClicked: groupRequested("tBu") }
    Button { text: "Ph"; width: 66; height: 58; font.pixelSize: 20; enabled: selectedAtom >= 0; onClicked: groupRequested("Ph") }
    Button { objectName: "lonePairButton"; text: "LP"; width: 56; height: 58; font.pixelSize: 20; enabled: selectedAtom >= 0; onClicked: lonePairRequested() }
    Button { objectName: "radicalButton"; text: "•"; width: 56; height: 58; font.pixelSize: 24; enabled: selectedAtom >= 0; onClicked: radicalRequested() }
    Button {
        objectName: "circledChargesButton"
        text: "(+)"
        checkable: true
        checked: circledCharges
        width: 66; height: 58; font.pixelSize: 20
        onClicked: circledChargesToggled(checked)
    }
    Label { text: "Abbrev"; font.pixelSize: 17; font.bold: true; height: 44; verticalAlignment: Text.AlignVCenter; width: 66 }
    Repeater {
        model: molecule.abbreviationButtons()
        delegate: Button {
            text: modelData.label
            width: Math.max(46, text.length * 12 + 18)
            height: 44
            padding: 4
            font.pixelSize: 16
            enabled: selectedAtom >= 0
            onClicked: abbreviationRequested(modelData.name)
        }
    }
}
