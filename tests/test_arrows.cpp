#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SvgWriter.h"

#include <QFile>
#include <QtMath>
#include <QtTest>
#include <QTemporaryDir>

class ArrowsTest : public QObject {
    Q_OBJECT

    static QVariantMap pt(double x, double y) { return {{"x", x}, {"y", y}}; }
    static QVariantMap itemById(const ReactionLayer &layer, int id) {
        for (const QVariant &value : layer.items())
            if (value.toMap().value("id").toInt() == id) return value.toMap();
        return {};
    }

private slots:
    void kindsRoundTripThroughRcd() {
        QTemporaryDir dir;
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const QStringList kinds{"forward", "equilibrium", "retro", "resonance", "dashed", "noreaction", "curved", "fishhook"};
        int y = 0;
        for (const QString &kind : kinds) {
            QVERIFY(layer.addArrowKind(kind, 0, y, 100, y, 50, y - 40) >= 0);
            y += 60;
        }
        const QString path = dir.filePath("kinds.rcd");
        QVERIFY(layer.saveToFile(path));
        Molecule other;
        ReactionLayer loaded(&other);
        QVERIFY(loaded.loadFromFile(path));
        const QVariantList items = loaded.items();
        QCOMPARE(items.size(), kinds.size());
        for (int i = 0; i < kinds.size(); ++i) {
            QCOMPARE(items[i].toMap().value("kind").toString(), kinds[i]);
            if (ReactionLayer::isCurvedKind(kinds[i])) {
                QCOMPARE(items[i].toMap().value("cx").toDouble(), 50.0);
                QCOMPARE(items[i].toMap().value("cy").toDouble(), i * 60.0 - 40);
            }
        }
    }

    void oldFilesLoadAsForward() {
        QTemporaryDir dir;
        Molecule molecule;
        ReactionLayer layer(&molecule);
        layer.addArrow(0, 0, 100, 0);
        const QString path = dir.filePath("old.rcd");
        QVERIFY(layer.saveToFile(path));
        // Strip the kind field to mimic a file written before arrow kinds existed.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QByteArray data = file.readAll();
        data.replace("\"kind\": \"forward\",", "");
        QVERIFY(!data.contains("\"kind\""));
        file.resize(0);
        file.write(data);
        file.close();
        Molecule other;
        ReactionLayer loaded(&other);
        QVERIFY(loaded.loadFromFile(path));
        QCOMPARE(loaded.items().size(), 1);
        QCOMPARE(loaded.items()[0].toMap().value("kind").toString(), QString("forward"));
    }

    void curvedHitTesting() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        // Chord along y = 0, control above: apex of the curve is at y = -40.
        const int id = layer.addArrowKind("curved", 0, 0, 200, 0, 100, -80);
        QCOMPARE(layer.itemAt(100, -40, 6), id);
        QCOMPARE(layer.itemAt(100, 0, 6), -1);       // the chord is not the curve
        QCOMPARE(layer.itemAt(0, 0, 6), id);
        // A straight arrow is still hit along its line.
        const int straight = layer.addArrowKind("forward", 0, 200, 200, 200);
        QCOMPARE(layer.itemAt(100, 200, 6), straight);
    }

    void movingCurvedArrowMovesControlPoint() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int id = layer.addArrowKind("fishhook", 0, 0, 100, 0, 50, -60);
        QVERIFY(layer.translateItem(id, 30, 20));
        const QVariantMap item = itemById(layer, id);
        QCOMPARE(item.value("x1").toDouble(), 30.0);
        QCOMPARE(item.value("cx").toDouble(), 80.0);
        QCOMPARE(item.value("cy").toDouble(), -40.0);
    }

    void dragPathBendsTowardDrag() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        QVariantList path;
        // Left to right, bulging downwards (positive y) to y = 30.
        for (int i = 0; i <= 10; ++i) path.append(pt(i * 20, 30 * (1 - qPow((i - 5) / 5.0, 2))));
        const int id = layer.addArrowFromPath("curved", path);
        QVERIFY(id >= 0);
        const QVariantMap item = itemById(layer, id);
        QCOMPARE(item.value("kind").toString(), QString("curved"));
        QVERIFY(item.value("cy").toDouble() > 30.0);   // control beyond the apex
        QCOMPARE(item.value("cx").toDouble(), 100.0);
        // The curve actually passes near the drawn apex.
        QCOMPARE(layer.itemAt(100, 30, 3), id);
        // One arrow is one undo step.
        QVERIFY(layer.undo());
        QCOMPARE(layer.items().size(), 0);
    }

    void straightKindsIgnoreBend() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        QVariantList path{pt(0, 0), pt(50, 30), pt(100, 0)};
        const int id = layer.addArrowFromPath("equilibrium", path);
        QVERIFY(id >= 0);
        QCOMPARE(itemById(layer, id).value("y2").toDouble(), 0.0);
    }

    void svgDrawsEveryKind() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        for (const char *kind : {"forward", "equilibrium", "retro", "resonance", "dashed", "noreaction", "curved", "fishhook"})
            layer.addArrowKind(kind, 0, 0, 100, 0, 50, -40);
        const QString svg = SvgWriter::write(molecule, layer.items());
        auto group = [&](const QString &kind) {
            const int start = svg.indexOf("<g class=\"arrow-" + kind + "\">");
            return start < 0 ? QString() : svg.mid(start, svg.indexOf("</g>", start) - start);
        };
        QCOMPARE(group("forward").count("<line"), 1);
        QCOMPARE(group("forward").count("<polyline"), 1);
        QCOMPARE(group("equilibrium").count("<line"), 2);
        QCOMPARE(group("equilibrium").count("<polyline"), 2);
        QCOMPARE(group("retro").count("<line"), 2);
        QCOMPARE(group("resonance").count("<polyline"), 2);
        QVERIFY(group("dashed").contains("stroke-dasharray"));
        QCOMPARE(group("noreaction").count("<line"), 3);
        QVERIFY(group("curved").contains("<path"));
        QVERIFY(group("curved").contains(" Q50,-40 "));
        QVERIFY(group("fishhook").contains("<path"));
        QCOMPARE(group("curved").count("<polyline"), 1);
        // Full head has three points, fishhook two (half head).
        auto points = [&](const QString &kind) {
            const QString g = group(kind);
            const int at = g.indexOf("points=\"") + 8;
            return g.mid(at, g.indexOf('"', at) - at).split(' ').size();
        };
        QCOMPARE(points("curved"), 3);
        QCOMPARE(points("fishhook"), 2);
    }

    void labelSnapsAboveAndBelow() {
        Molecule molecule;
        ReactionLayer layer(&molecule);
        layer.addArrow(0, 100, 200, 100);
        const int above = layer.addLabelSnapped("THF", 60, 85);
        QVariantMap item = itemById(layer, above);
        QCOMPARE(item.value("x1").toDouble(), 100.0);
        QVERIFY(item.value("y1").toDouble() < 100.0);
        const int below = layer.addLabelSnapped("rt", 150, 120);
        item = itemById(layer, below);
        QCOMPARE(item.value("x1").toDouble(), 100.0);
        QVERIFY(item.value("y1").toDouble() > 100.0);
        // Far from any arrow the label stays where it was tapped.
        const int free = layer.addLabelSnapped("free", 60, 300);
        QCOMPARE(itemById(layer, free).value("y1").toDouble(), 300.0);
        // Curved arrows do not attract labels.
        layer.addArrowKind("curved", 0, 500, 200, 500, 100, 440);
        const int curved = layer.addLabelSnapped("x", 100, 470);
        QCOMPARE(itemById(layer, curved).value("y1").toDouble(), 470.0);
    }

    void compoundNumbersAreBold() {
        QVERIFY(ReactionLayer::isCompoundNumber("1"));
        QVERIFY(ReactionLayer::isCompoundNumber("12"));
        QVERIFY(ReactionLayer::isCompoundNumber("2a"));
        QVERIFY(!ReactionLayer::isCompoundNumber("2ab"));
        QVERIFY(!ReactionLayer::isCompoundNumber("2A"));
        QVERIFY(!ReactionLayer::isCompoundNumber("a2"));
        QVERIFY(!ReactionLayer::isCompoundNumber("THF"));
        QVERIFY(!ReactionLayer::isCompoundNumber(""));
        Molecule molecule;
        ReactionLayer layer(&molecule);
        const int one = layer.addLabel("2a", 0, 0);
        const int word = layer.addLabel("THF", 50, 0);
        QVERIFY(itemById(layer, one).value("bold").toBool());
        QVERIFY(!itemById(layer, word).value("bold").toBool());
        const QString svg = SvgWriter::write(molecule, layer.items());
        QVERIFY(svg.contains("font-weight=\"bold\">2a</text>"));
        QVERIFY(svg.contains("stroke=\"none\">THF</text>"));
    }
};

QTEST_MAIN(ArrowsTest)
#include "test_arrows.moc"
