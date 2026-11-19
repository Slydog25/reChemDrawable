#include "core/CdxmlWriter.h"
#include "core/Document.h"
#include "core/Molecule.h"
#include "core/ReactionLayer.h"
#include "core/SceneRenderer.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

#include <memory>

class ExportsTest : public QObject {
    Q_OBJECT

    std::unique_ptr<Molecule> m_molecule;
    std::unique_ptr<ReactionLayer> m_layer;

    static QByteArray readAll(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
    // Pixel of the PNG at a model point.
    QRgb pixelAt(const QImage &image, double x, double y, double scale = SceneRenderer::PngScale) const {
        const QRectF box = SceneRenderer::contentBounds(*m_molecule, m_layer->items())
                               .adjusted(-SceneRenderer::ExportMargin, -SceneRenderer::ExportMargin,
                                         SceneRenderer::ExportMargin, SceneRenderer::ExportMargin);
        return image.pixel(qRound((x - box.left()) * scale), qRound((y - box.top()) * scale));
    }
    // True when any pixel within r model units of (x, y) is dark.
    bool inkNear(const QImage &image, double x, double y, double r = 4) const {
        for (double dx = -r; dx <= r; dx += 1)
            for (double dy = -r; dy <= r; dy += 1)
                if (qGray(pixelAt(image, x + dx, y + dy)) < 128) return true;
        return false;
    }

private slots:
    void init() {
        m_molecule = std::make_unique<Molecule>();
        m_layer = std::make_unique<ReactionLayer>(m_molecule.get());
    }

private:
    // Small molecule: C=C, N(+), a wedge to O and a compound number.
    void buildSample() {
        const int a = m_molecule->addAtom(0, 0, "C");
        const int b = m_molecule->addAtom(48, 0, "C");
        const int c = m_molecule->addAtom(96, 0, "N");
        const int d = m_molecule->addAtom(48, 48, "O");
        QVERIFY(m_molecule->addBond(a, b, 2) > 0);
        QVERIFY(m_molecule->addBond(b, c, 1) > 0);
        const int wedge = m_molecule->addBond(b, d, 1);
        QVERIFY(m_molecule->setBondStyle(wedge, "wedge"));
        QVERIFY(m_molecule->setAtomCharge(c, 1));
        m_layer->addLabel("1", 48, 120);
    }

private slots:
    void pngScalesAndDrawsAtoms() {
        buildSample();
        const QImage one = SceneRenderer::renderImage(*m_molecule, m_layer->items(), 1.0);
        const QImage big = SceneRenderer::renderImage(*m_molecule, m_layer->items());
        QVERIFY(!one.isNull());
        QVERIFY(qAbs(big.width() - one.width() * SceneRenderer::PngScale) < 4);
        QVERIFY(qAbs(big.height() - one.height() * SceneRenderer::PngScale) < 4);
        QCOMPARE(big.dotsPerMeterX(), qRound(SceneRenderer::PngScale * 96 / 0.0254));
        // White margin in the corner, ink on the double bond (midpoint of the C=C bond line) and wedge.
        QCOMPARE(big.pixel(1, 1), qRgb(255, 255, 255));
        QVERIFY(inkNear(big, 24, -4, 2));
        QVERIFY(inkNear(big, 48, 24, 2)); // wedge body
        QVERIFY(inkNear(big, 96, 0, 8));  // N label
    }

    void emptySceneRendersNothing() {
        QVERIFY(SceneRenderer::renderImage(*m_molecule, m_layer->items()).isNull());
    }

    void pdfIsAVectorPage() {
        buildSample();
        QTemporaryDir dir;
        const QString path = dir.filePath("a.pdf");
        QVERIFY(SceneRenderer::writePdf(path, *m_molecule, m_layer->items(), "sample"));
        const QByteArray pdf = readAll(path);
        QVERIFY(pdf.startsWith("%PDF"));
        QVERIFY(pdf.contains("/Type /Page"));
        QVERIFY(pdf.contains("/MediaBox"));
        QVERIFY(pdf.trimmed().endsWith("%%EOF"));
    }

    void cdxmlIsWellFormedWithExpectedContent() {
        buildSample();
        m_layer->addArrowKind("equilibrium", 0, 200, 100, 200);
        m_layer->addArrowKind("forward", 0, 240, 100, 240);
        m_layer->addArrowKind("curved", 0, 280, 100, 280, 50, 250);
        const QString xml = CdxmlWriter::write(*m_molecule, m_layer->items());
        QXmlStreamReader reader(xml);
        int nodes = 0, bonds = 0, texts = 0, arrows = 0, curves = 0, graphics = 0;
        bool sawCharge = false, sawNitrogen = false, sawWedge = false, sawDouble = false, sawBoldLabel = false;
        QStringList ids;
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement()) continue;
            const auto attrs = reader.attributes();
            if (attrs.hasAttribute("id") && reader.name() != QLatin1String("font")) ids << attrs.value("id").toString(); // font ids are their own namespace
            const QString name = reader.name().toString();
            if (name == QLatin1String("CDXML")) QCOMPARE(attrs.value("BondLength").toString(), QString("14.4"));
            if (name == QLatin1String("n")) {
                ++nodes;
                QVERIFY(attrs.hasAttribute("p"));
                if (attrs.value(QLatin1String("Charge")) == QLatin1String("1")) sawCharge = true;
                if (attrs.value(QLatin1String("Element")) == QLatin1String("7")) sawNitrogen = true;
            } else if (name == QLatin1String("b")) {
                ++bonds;
                if (attrs.value(QLatin1String("Display")) == QLatin1String("WedgeBegin")) sawWedge = true;
                if (attrs.value(QLatin1String("Order")) == QLatin1String("2")) sawDouble = true;
            } else if (name == QLatin1String("t")) {
                ++texts;
            } else if (name == QLatin1String("s")) {
                if (attrs.value(QLatin1String("face")) == QLatin1String("1")) sawBoldLabel = true;
            } else if (name == QLatin1String("arrow")) ++arrows;
            else if (name == QLatin1String("curve")) ++curves;
            else if (name == QLatin1String("graphic")) {
                ++graphics;
                QCOMPARE(attrs.value("ArrowType").toString(), QString("Equilibrium"));
            }
        }
        QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
        QCOMPARE(nodes, 4);
        QCOMPARE(bonds, 3);
        QVERIFY(sawCharge && sawNitrogen && sawWedge && sawDouble && sawBoldLabel);
        QCOMPARE(arrows, 1);
        QCOMPARE(curves, 1);
        QCOMPARE(graphics, 1);
        QVERIFY(texts >= 3); // N, O labels and the "1"
        QCOMPARE(ids.size(), QSet<QString>(ids.begin(), ids.end()).size()); // unique ids
        // Positions are points: 48 model units = 14.4 pt.
        QVERIFY(xml.contains("p=\"14.40 0.00\""));
    }

    void cdxmlWritesAbbreviationsAsNicknames() {
        const int a = m_molecule->addAtom(0, 0, "C");
        const int ome = m_molecule->applyAbbreviation(a, "OMe");
        QVERIFY(ome >= 0);
        const QString xml = CdxmlWriter::write(*m_molecule, {});
        QVERIFY(xml.contains("NodeType=\"Nickname\""));
        QVERIFY(xml.contains(">OMe<") || xml.contains(">MeO<"));
        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) reader.readNext();
        QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    }

    void libraryExportWritesThreeFiles() {
        buildSample();
        QTemporaryDir docs, library;
        Document document(m_molecule.get(), m_layer.get(), docs.path());
        document.setLibraryDirectory(library.path());
        QSignalSpy messages(&document, &Document::message);
        const QString pdfPath = document.sendToLibrary();
        QVERIFY(!pdfPath.isEmpty());
        const QString uuid = QFileInfo(pdfPath).completeBaseName();
        QVERIFY(!QUuid::fromString(uuid).isNull());
        QVERIFY(readAll(pdfPath).startsWith("%PDF"));
        QStringList files = QDir(library.path()).entryList(QDir::Files, QDir::Name);
        QCOMPARE(files, (QStringList{uuid + ".content", uuid + ".metadata", uuid + ".pdf"}));
        const QJsonObject metadata = QJsonDocument::fromJson(readAll(library.filePath(uuid + ".metadata"))).object();
        QCOMPARE(metadata.value("type").toString(), QString("DocumentType"));
        QCOMPARE(metadata.value("visibleName").toString(), QString("Untitled"));
        QCOMPARE(metadata.value("parent").toString(), QString());
        QVERIFY(metadata.contains("lastModified"));
        QVERIFY(metadata.value("lastModified").toString().toLongLong() > 0);
        QVERIFY(metadata.contains("version") && metadata.contains("deleted"));
        const QJsonObject content = QJsonDocument::fromJson(readAll(library.filePath(uuid + ".content"))).object();
        QCOMPARE(content.value("fileType").toString(), QString("pdf"));
        QCOMPARE(content.value("pageCount").toInt(), 1);
        QVERIFY(messages.last().first().toString().contains("xochitl"));
    }

    void libraryExportReportsMissingFolderAndEmptyDrawing() {
        QTemporaryDir docs, library;
        Document document(m_molecule.get(), m_layer.get(), docs.path());
        document.setLibraryDirectory(library.filePath("nope"));
        QVERIFY(document.sendToLibrary().isEmpty());
        document.setLibraryDirectory(library.path());
        QVERIFY(document.sendToLibrary().isEmpty()); // nothing drawn
        QVERIFY(QDir(library.path()).entryList(QDir::Files).isEmpty()); // no stray PDF left behind
    }

    void documentExportsPngPdfCdxml() {
        buildSample();
        QTemporaryDir docs;
        Document document(m_molecule.get(), m_layer.get(), docs.path());
        QVERIFY(QImage(document.exportPng()).width() > 100);
        QVERIFY(readAll(document.exportPdf()).startsWith("%PDF"));
        QVERIFY(readAll(document.exportCdxml()).contains("<CDXML"));
    }

    // Each arrow kind must put ink along its shaft; a no-reaction arrow also crosses at its middle.
    void everyArrowKindDrawsInk() {
        const QStringList kinds = {"forward", "equilibrium", "retro", "resonance", "dashed", "noreaction", "curved", "fishhook"};
        for (const QString &kind : kinds) {
            Molecule molecule;
            ReactionLayer layer(&molecule);
            layer.addArrowKind(kind, 0, 0, 200, 0, 100, -60);
            const QImage image = SceneRenderer::renderImage(molecule, layer.items(), 2.0);
            QVERIFY2(!image.isNull(), qPrintable(kind));
            const QRectF box = SceneRenderer::contentBounds(molecule, layer.items()).adjusted(-32, -32, 32, 32);
            auto dark = [&](double x, double y) {
                for (double dx = -6; dx <= 6; ++dx)
                    for (double dy = -6; dy <= 6; ++dy) {
                        const int px = qRound((x + dx - box.left()) * 2), py = qRound((y + dy - box.top()) * 2);
                        if (image.rect().contains(px, py) && qGray(image.pixel(px, py)) < 128) return true;
                    }
                return false;
            };
            QVERIFY2(dark(200, 0) || dark(190, 0), qPrintable(kind + " head")); // tip / shaft end
            if (kind == "curved" || kind == "fishhook") QVERIFY2(dark(100, -30), qPrintable(kind + " apex"));
            else QVERIFY2(dark(60, kind == "equilibrium" ? -4 : 0), qPrintable(kind + " shaft"));
            if (kind == "noreaction") QVERIFY(dark(100, 8));
            // Far from everything stays white.
            QCOMPARE(image.pixel(1, 1), qRgb(255, 255, 255));
        }
    }

    void circledChargesLonePairsAndRadicalsRender() {
        const int o = m_molecule->addAtom(0, 0, "O");
        QVERIFY(m_molecule->setAtomCharge(o, -1));
        m_molecule->setCircledCharges(true);
        m_molecule->cycleLonePairs(o);
        const int c = m_molecule->addAtom(60, 0, "C");
        QVERIFY(m_molecule->cycleRadical(c));
        const QImage image = SceneRenderer::renderImage(*m_molecule, {}, 3.0);
        int dark = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qGray(image.pixel(x, y)) < 128) ++dark;
        QVERIFY(dark > 200);
    }
};

QTEST_MAIN(ExportsTest)
#include "test_exports.moc"
