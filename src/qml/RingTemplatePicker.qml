import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flow {
    property string selectedRing: "cyclohexane"
    signal ringSelected(string ring)
    spacing: 8
    Label { text: "Rings"; font.pixelSize: 19; font.bold: true; height: 58; verticalAlignment: Text.AlignVCenter }
    Repeater {
        model: [["cyclopropane", "3"], ["cyclobutane", "4"], ["cyclopentane", "5"], ["cyclohexane", "6"],
                ["cycloheptane", "7"], ["cyclooctane", "8"], ["benzene", "Bz"], ["chair", "Chair"],
                ["pyridine", "Py"], ["pyrimidine", "Pm"], ["furan", "Fu"], ["thiophene", "Th"],
                ["pyrrole", "Pyl"], ["imidazole", "Im"], ["indole", "Ind"]]
        delegate: Button {
            text: modelData[1]
            width: modelData[1].length > 3 ? 84 : modelData[1].length > 1 ? 66 : 52
            height: 58
            font.pixelSize: 22
            checkable: true
            checked: selectedRing === modelData[0]
            onClicked: ringSelected(modelData[0])
        }
    }
}
