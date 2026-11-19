// Renders every toolbar icon on one sheet for visual review. Not part of the
// test suite; run it with the QML test runner:
//   QT_QPA_PLATFORM=offscreen ICON_SHEET=icons.png build-pc/qml_tests -input tools/iconsheet
import QtQuick
import QtTest
import "../../src/qml"

Rectangle {
    id: sheet
    width: 8 * 120 + 20
    height: Math.ceil(names.length / 8) * 130 + 20
    color: "#f7f5ed"
    property var names: ["draw", "select", "move", "pan", "bond", "style", "ring", "chain", "arrow", "text", "erase",
                         "undo", "redo", "grid", "zoomIn", "zoomOut", "fit", "new", "load", "save", "mol",
                         "svg", "smiles", "clean", "rotate", "flipH", "flipV", "alignH", "alignV", "copy",
                         "paste", "delete", "clear", "exit", "duplicate", "bold", "stereo",
                         "akForward", "akEquilibrium", "akRetro", "akResonance", "akDashed", "akNoReaction", "akCurved", "akFishhook", "check", "tidy"]
    Grid {
        x: 10; y: 10
        columns: 8
        Repeater {
            model: sheet.names
            delegate: Item {
                width: 120; height: 130
                Rectangle {
                    x: 7; y: 4; width: 106; height: 96; radius: 9
                    color: "white"; border.color: "#171717"; border.width: 2
                    ToolbarIcon { anchors.centerIn: parent; name: modelData; scale: 1.8 }
                }
                Text { y: 104; width: 120; horizontalAlignment: Text.AlignHCenter; text: modelData; font.pixelSize: 15 }
            }
        }
    }
    TestCase {
        name: "IconSheet"
        when: windowShown
        function test_render() {
            wait(300)
            const image = grabImage(sheet)
            image.save(Qt.application.arguments.indexOf("-o") >= 0 ? "" : "/workspace/build-pc/icon-sheet.png")
        }
    }
}
