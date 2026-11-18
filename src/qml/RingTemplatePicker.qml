import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flow {
    property string selectedRing: "cyclohexane"
    signal ringSelected(string ring)
    spacing: 8
    Label { text: "Rings"; font.pixelSize: 19; font.bold: true; height: 58; verticalAlignment: Text.AlignVCenter }
    Repeater {
        model: [["cyclopropane", "3"], ["cyclobutane", "4"], ["cyclopentane", "5"], ["cyclohexane", "6"], ["benzene", "Bz"]]
        delegate: Button {
            text: modelData[1]
            width: modelData[1] === "Bz" ? 76 : 56
            height: 58
            font.pixelSize: 22
            checkable: true
            checked: selectedRing === modelData[0]
            onClicked: ringSelected(modelData[0])
        }
    }
}
