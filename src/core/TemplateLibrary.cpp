#include "TemplateLibrary.h"

#include "Document.h"
#include "GeometryEngine.h"
#include "Molecule.h"
#include "SmilesReader.h"
#include "Stereo.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QtMath>

namespace {
constexpr double L = GeometryEngine::StandardBondLength;

// Small helper for hand-placed templates. Atom ids are 1..n in creation order;
// coordinates are given in bond lengths.
struct Builder {
    QVector<Atom> atoms;
    QVector<Bond> bonds;

    int atom(const QString &element, double x, double y) {
        Atom a;
        a.id = atoms.size() + 1;
        a.element = element;
        a.x = x * L;
        a.y = y * L;
        atoms.append(a);
        return a.id;
    }
    int bond(int a, int b, int order = 1, const QString &style = QStringLiteral("plain")) {
        Bond bd;
        bd.id = bonds.size() + 1;
        bd.atomA = a;
        bd.atomB = b;
        bd.order = order;
        bd.style = style;
        bonds.append(bd);
        return bd.id;
    }
    int attach(int from, const QString &element, double x, double y, int order = 1) {
        const int id = atom(element, x, y);
        bond(from, id, order);
        return id;
    }
};

void centre(QVector<Atom> &atoms) {
    if (atoms.isEmpty()) return;
    double cx = 0, cy = 0;
    for (const Atom &a : std::as_const(atoms)) { cx += a.x; cy += a.y; }
    cx /= atoms.size();
    cy /= atoms.size();
    for (Atom &a : atoms) { a.x -= cx; a.y -= cy; }
}

const Atom *atomById(const QVector<Atom> &atoms, int id) {
    for (const Atom &a : atoms) if (a.id == id) return &a;
    return nullptr;
}

QVector<int> neighbours(const QVector<Bond> &bonds, int id) {
    QVector<int> result;
    for (const Bond &b : bonds) {
        if (b.atomA == id) result.append(b.atomB);
        else if (b.atomB == id) result.append(b.atomA);
    }
    return result;
}

Bond *bondBetween(QVector<Bond> &bonds, int a, int b) {
    for (Bond &bond : bonds)
        if ((bond.atomA == a && bond.atomB == b) || (bond.atomA == b && bond.atomB == a)) return &bond;
    return nullptr;
}

void setStereoStyle(Bond &bond, int fromAtom, int toAtom, bool wedge) {
    bond.atomA = fromAtom;
    bond.atomB = toAtom;
    bond.style = wedge ? QStringLiteral("wedge") : QStringLiteral("dash");
    bond.stereo = wedge ? 1 : 6;
}

QString labelAt(const QVector<Atom> &atoms, const QVector<Bond> &bonds, int atomId) {
    Molecule m;
    m.replaceContent(atoms, bonds);
    for (const Stereo::Descriptor &d : Stereo::compute(m))
        if (d.atomId == atomId) return d.label;
    return QString();
}

// Chooses wedge or dash for the bond from->to so that `from` gets the wanted CIP label.
void orientStereo(QVector<Atom> &atoms, QVector<Bond> &bonds, int from, int to, const QString &wanted) {
    Bond *bond = bondBetween(bonds, from, to);
    if (!bond) return;
    setStereoStyle(*bond, from, to, true);
    if (labelAt(atoms, bonds, from) == wanted) return;
    setStereoStyle(*bond, from, to, false);
}

bool hasElement(const QVector<Atom> &atoms, int id, const QString &element) {
    const Atom *a = atomById(atoms, id);
    return a && a->element == element;
}

// A carboxylic acid carbon: a double-bonded O and a single-bonded terminal O.
bool isCarboxylC(const QVector<Atom> &atoms, const QVector<Bond> &bonds, int id) {
    if (!hasElement(atoms, id, QStringLiteral("C"))) return false;
    bool doubleO = false, singleO = false;
    for (const Bond &b : bonds) {
        const int other = b.atomA == id ? b.atomB : b.atomB == id ? b.atomA : -1;
        if (other < 0 || !hasElement(atoms, other, QStringLiteral("O"))) continue;
        if (b.order == 2) doubleO = true;
        else if (neighbours(bonds, other).size() == 1) singleO = true;
    }
    return doubleO && singleO;
}

TemplateLibrary::Fragment fromSmiles(const QString &id, const QString &category, const QString &name,
                                     const QString &label, const QString &smiles) {
    TemplateLibrary::Fragment f;
    f.id = id; f.category = category; f.name = name; f.label = label;
    QString error;
    SmilesReader::build(smiles, 0, 0, f.atoms, f.bonds, &error);
    centre(f.atoms);
    return f;
}

// One amino acid: SMILES laid out, turned so N - C(alpha) - C(=O)OH zig-zags left to right with
// C(alpha) on top, then wedge or dash chosen so the app's own CIP perception reports the L
// configuration (S, except cysteine R; threonine C-beta R, isoleucine C-beta S).
TemplateLibrary::Fragment aminoAcid(const QString &code, const QString &name, const QString &smiles,
                                    bool cysteine = false, const QString &betaWanted = QString()) {
    TemplateLibrary::Fragment f = fromSmiles(QStringLiteral("amino:") + code.toLower(), QStringLiteral("amino"), name, code, smiles);
    int alpha = -1, carboxyl = -1, nitrogen = -1;
    for (const Atom &a : std::as_const(f.atoms)) {
        if (a.element != QStringLiteral("C")) continue;
        int n = -1, c = -1;
        for (int other : neighbours(f.bonds, a.id)) {
            if (hasElement(f.atoms, other, QStringLiteral("N"))) n = other;
            else if (isCarboxylC(f.atoms, f.bonds, other)) c = other;
        }
        if (n >= 0 && c >= 0) { alpha = a.id; nitrogen = n; carboxyl = c; break; }
    }
    if (alpha < 0) return f;
    const Atom N = *atomById(f.atoms, nitrogen), C = *atomById(f.atoms, carboxyl);
    const double angle = qAtan2(C.y - N.y, C.x - N.x), cs = qCos(-angle), sn = qSin(-angle);
    for (Atom &a : f.atoms) {
        const double x = a.x * cs - a.y * sn, y = a.x * sn + a.y * cs;
        a.x = x; a.y = y;
    }
    const double lineY = (atomById(f.atoms, nitrogen)->y + atomById(f.atoms, carboxyl)->y) / 2.0;
    if (atomById(f.atoms, alpha)->y > lineY) for (Atom &a : f.atoms) a.y = -a.y;
    centre(f.atoms);

    int side = -1;
    for (int other : neighbours(f.bonds, alpha)) if (other != nitrogen && other != carboxyl) side = other;
    if (side < 0) return f; // glycine: no stereocentre
    orientStereo(f.atoms, f.bonds, alpha, side, cysteine ? QStringLiteral("R") : QStringLiteral("S"));
    if (!betaWanted.isEmpty()) {
        // Second stereocentre (Thr, Ile): wedge to the smallest branch, heavier atom first.
        int branch = -1, best = 1 << 20;
        for (int other : neighbours(f.bonds, side)) {
            if (other == alpha) continue;
            const int score = neighbours(f.bonds, other).size() * 100 - Stereo::rankingAtomicNumber(atomById(f.atoms, other)->element);
            if (score < best) { best = score; branch = other; }
        }
        if (branch >= 0) orientStereo(f.atoms, f.bonds, side, branch, betaWanted);
    }
    return f;
}

TemplateLibrary::Fragment projection(const QString &id, const QString &name, const QString &label, const Builder &b) {
    TemplateLibrary::Fragment f;
    f.id = QStringLiteral("proj:") + id; f.category = QStringLiteral("projections"); f.name = name; f.label = label;
    f.atoms = b.atoms; f.bonds = b.bonds;
    centre(f.atoms);
    return f;
}

// Staggered Newman projection of an ethane-type C-C: front bonds up / lower right / lower left,
// rear bonds from the circle at -30, -150 and -270 degrees. All six positions are hydrogens.
Builder newman() {
    Builder b;
    const double r = 56.0 / L;
    const int front = b.atom(QStringLiteral("C"), 0, 0);
    const int rear = b.atom(QStringLiteral("C"), 0, 2.0 / L); // 2 px behind, so the bond has a direction
    b.bond(front, rear, 1, QStringLiteral("newman"));
    for (int i = 0; i < 3; ++i) {
        const double a = qDegreesToRadians(-30.0 - 120.0 * i);
        const int h = b.atom(QStringLiteral("H"), r * qCos(a), r * qSin(a));
        b.bond(rear, h, 1, QStringLiteral("newmanback"));
    }
    for (int i = 0; i < 3; ++i) {
        const double a = qDegreesToRadians(-90.0 + 120.0 * i);
        const int h = b.atom(QStringLiteral("H"), r * qCos(a), r * qSin(a));
        b.bond(front, h);
    }
    return b;
}

// Fischer projection: carbons stacked one bond apart. Each entry of hydroxylRight is a carbon with
// OH on the right (true) or left (false) and H on the other side.
Builder fischer(bool aldehydeTop, bool alcoholBottom, const QVector<bool> &hydroxylRight) {
    Builder b;
    int previous = -1, row = 0;
    if (aldehydeTop) {
        const int c = b.atom(QStringLiteral("C"), 0, row++);
        b.attach(c, QStringLiteral("O"), 0, -1, 2);
        previous = c;
    }
    for (bool right : hydroxylRight) {
        const int c = b.atom(QStringLiteral("C"), 0, row);
        if (previous >= 0) b.bond(previous, c);
        b.attach(c, QStringLiteral("H"), right ? -1 : 1, row);
        b.attach(c, QStringLiteral("O"), right ? 1 : -1, row);
        previous = c;
        ++row;
    }
    if (alcoholBottom) {
        const int c = b.atom(QStringLiteral("C"), 0, row);
        if (previous >= 0) b.bond(previous, c);
        b.attach(c, QStringLiteral("O"), 0, row + 1);
    }
    return b;
}

// Haworth pyranose: flat hexagon, O at the back right, anomeric carbon at the right, front edge
// bold. up[i] says whether the group on C1..C4 points up; C5 always carries CH2OH (up, D-series).
Builder haworthPyranose(const QVector<bool> &up) {
    Builder b;
    const int o = b.atom(QStringLiteral("O"), 0.75, -0.55);
    const int c1 = b.atom(QStringLiteral("C"), 1.5, 0);
    const int c2 = b.atom(QStringLiteral("C"), 0.75, 0.55);
    const int c3 = b.atom(QStringLiteral("C"), -0.75, 0.55);
    const int c4 = b.atom(QStringLiteral("C"), -1.5, 0);
    const int c5 = b.atom(QStringLiteral("C"), -0.75, -0.55);
    const QString bold = QStringLiteral("bold");
    b.bond(o, c1);
    b.bond(c1, c2, 1, bold);
    b.bond(c2, c3, 1, bold);
    b.bond(c3, c4, 1, bold);
    b.bond(c4, c5);
    b.bond(c5, o);
    const int ring[4] = {c1, c2, c3, c4};
    const double xs[4] = {1.5, 0.75, -0.75, -1.5}, ys[4] = {0, 0.55, 0.55, 0};
    for (int i = 0; i < 4; ++i) b.attach(ring[i], QStringLiteral("O"), xs[i], ys[i] + (up[i] ? -0.9 : 0.9));
    const int c6 = b.attach(c5, QStringLiteral("C"), -0.75, -1.55);
    b.attach(c6, QStringLiteral("O"), -1.62, -2.05);
    return b;
}

// Haworth beta-D-ribofuranose: O at the back, front edge bold, C4 carries CH2OH.
Builder haworthFuranose() {
    Builder b;
    const int o = b.atom(QStringLiteral("O"), 0, -0.65);
    const int c1 = b.atom(QStringLiteral("C"), 1.0, -0.1);
    const int c2 = b.atom(QStringLiteral("C"), 0.6, 0.65);
    const int c3 = b.atom(QStringLiteral("C"), -0.6, 0.65);
    const int c4 = b.atom(QStringLiteral("C"), -1.0, -0.1);
    const QString bold = QStringLiteral("bold");
    b.bond(o, c1);
    b.bond(c1, c2, 1, bold);
    b.bond(c2, c3, 1, bold);
    b.bond(c3, c4, 1, bold);
    b.bond(c4, o);
    b.attach(c1, QStringLiteral("O"), 1.0, -1.0);
    b.attach(c2, QStringLiteral("O"), 0.6, 1.5);
    b.attach(c3, QStringLiteral("O"), -0.6, 1.5);
    const int c5 = b.attach(c4, QStringLiteral("C"), -1.0, -1.1);
    b.attach(c5, QStringLiteral("O"), -1.87, -1.6);
    return b;
}

QVector<TemplateLibrary::Fragment> buildAll() {
    using F = TemplateLibrary::Fragment;
    const auto S = [](const char *s) { return QString::fromUtf8(s); };
    QVector<F> all;
    all.append(projection(S("newman"), S("Newman, staggered"), S("Newman"), newman()));
    all.append(projection(S("fischer2"), S("Fischer, 2 C"), S("Fischer 2C"), fischer(false, false, {true, true})));
    all.append(projection(S("dglyceraldehyde"), S("D-Glyceraldehyde"), S("Fischer 3C"), fischer(true, true, {true})));
    all.append(projection(S("lglyceraldehyde"), S("L-Glyceraldehyde"), S("Fischer 3C"), fischer(true, true, {false})));
    all.append(projection(S("derythrose"), S("D-Erythrose"), S("Fischer 4C"), fischer(true, true, {true, true})));
    all.append(projection(S("dthreose"), S("D-Threose"), S("Fischer 4C"), fischer(true, true, {false, true})));
    // alpha-D-glucopyranose: OH down at C1, C2, C4, up at C3. Beta flips C1.
    all.append(projection(S("alphaglucose"), S("α-D-Glucopyranose"), S("Haworth"), haworthPyranose({false, false, true, false})));
    all.append(projection(S("betaglucose"), S("β-D-Glucopyranose"), S("Haworth"), haworthPyranose({true, false, true, false})));
    all.append(projection(S("ribofuranose"), S("β-D-Ribofuranose"), S("Haworth"), haworthFuranose()));

    all.append(aminoAcid(S("Gly"), S("Glycine"), S("NCC(=O)O")));
    all.append(aminoAcid(S("Ala"), S("Alanine"), S("NC(C)C(=O)O")));
    all.append(aminoAcid(S("Val"), S("Valine"), S("NC(C(C)C)C(=O)O")));
    all.append(aminoAcid(S("Leu"), S("Leucine"), S("NC(CC(C)C)C(=O)O")));
    all.append(aminoAcid(S("Ile"), S("Isoleucine"), S("NC(C(C)CC)C(=O)O"), false, S("S")));
    all.append(aminoAcid(S("Pro"), S("Proline"), S("OC(=O)C1CCCN1")));
    all.append(aminoAcid(S("Phe"), S("Phenylalanine"), S("NC(Cc1ccccc1)C(=O)O")));
    all.append(aminoAcid(S("Trp"), S("Tryptophan"), S("NC(Cc1c[nH]c2ccccc12)C(=O)O")));
    all.append(aminoAcid(S("Met"), S("Methionine"), S("NC(CCSC)C(=O)O")));
    all.append(aminoAcid(S("Ser"), S("Serine"), S("NC(CO)C(=O)O")));
    all.append(aminoAcid(S("Thr"), S("Threonine"), S("NC(C(C)O)C(=O)O"), false, S("R")));
    all.append(aminoAcid(S("Cys"), S("Cysteine"), S("NC(CS)C(=O)O"), true));
    all.append(aminoAcid(S("Tyr"), S("Tyrosine"), S("NC(Cc1ccc(O)cc1)C(=O)O")));
    all.append(aminoAcid(S("Asn"), S("Asparagine"), S("NC(CC(N)=O)C(=O)O")));
    all.append(aminoAcid(S("Gln"), S("Glutamine"), S("NC(CCC(N)=O)C(=O)O")));
    all.append(aminoAcid(S("Asp"), S("Aspartic acid"), S("NC(CC(=O)O)C(=O)O")));
    all.append(aminoAcid(S("Glu"), S("Glutamic acid"), S("NC(CCC(=O)O)C(=O)O")));
    all.append(aminoAcid(S("Lys"), S("Lysine"), S("NC(CCCCN)C(=O)O")));
    all.append(aminoAcid(S("Arg"), S("Arginine"), S("NC(CCCNC(N)=N)C(=O)O")));
    all.append(aminoAcid(S("His"), S("Histidine"), S("NC(Cc1cnc[nH]1)C(=O)O")));

    const QString bases = S("bases"), reagents = S("reagents");
    all.append(fromSmiles(S("base:a"), bases, S("Adenine"), S("A"), S("Nc1ncnc2[nH]cnc12")));
    all.append(fromSmiles(S("base:g"), bases, S("Guanine"), S("G"), S("Nc1nc2[nH]cnc2c(=O)[nH]1")));
    all.append(fromSmiles(S("base:c"), bases, S("Cytosine"), S("C"), S("Nc1cc[nH]c(=O)n1")));
    all.append(fromSmiles(S("base:t"), bases, S("Thymine"), S("T"), S("Cc1c[nH]c(=O)[nH]c1=O")));
    all.append(fromSmiles(S("base:u"), bases, S("Uracil"), S("U"), S("O=c1cc[nH]c(=O)[nH]1")));
    all.append(fromSmiles(S("reagent:dmap"), reagents, S("DMAP"), S("DMAP"), S("CN(C)c1ccncc1")));
    all.append(fromSmiles(S("reagent:dcc"), reagents, S("DCC"), S("DCC"), S("C1CCC(CC1)N=C=NC1CCCCC1")));
    all.append(fromSmiles(S("reagent:nbs"), reagents, S("NBS"), S("NBS"), S("O=C1CCC(=O)N1Br")));
    all.append(fromSmiles(S("reagent:tscl"), reagents, S("Tosyl chloride"), S("TsCl"), S("Cc1ccc(cc1)S(Cl)(=O)=O")));
    all.append(fromSmiles(S("reagent:boc2o"), reagents, S("Boc anhydride"), S("Boc2O"), S("CC(C)(C)OC(=O)OC(=O)OC(C)(C)C")));
    for (F &f : all) f.formula = TemplateLibrary::fragmentFormula(f.atoms, f.bonds);
    return all;
}

QJsonObject atomToJson(const Atom &a) {
    return {{QStringLiteral("element"), a.element}, {QStringLiteral("x"), a.x}, {QStringLiteral("y"), a.y},
            {QStringLiteral("charge"), a.formalCharge}, {QStringLiteral("isotope"), a.isotope},
            {QStringLiteral("radical"), a.radical}, {QStringLiteral("lonePairs"), a.lonePairs}};
}

QJsonObject bondToJson(const Bond &b, int indexA, int indexB) {
    return {{QStringLiteral("a"), indexA}, {QStringLiteral("b"), indexB}, {QStringLiteral("order"), b.order},
            {QStringLiteral("style"), b.style}, {QStringLiteral("stereo"), b.stereo},
            {QStringLiteral("doubleSide"), b.doubleSide}};
}
} // namespace

TemplateLibrary::TemplateLibrary(Molecule *molecule, const QString &directory, QObject *parent)
    : QObject(parent), m_molecule(molecule),
      m_directory(directory.isEmpty() ? Document::defaultDirectory() : directory) {
    loadMine();
}

QString TemplateLibrary::filePath() const { return QDir(m_directory).filePath(QStringLiteral("templates.json")); }

const QVector<TemplateLibrary::Fragment> &TemplateLibrary::builtins() {
    static const QVector<Fragment> all = buildAll();
    return all;
}

QString TemplateLibrary::fragmentFormula(const QVector<Atom> &atoms, const QVector<Bond> &bonds) {
    Molecule m;
    m.replaceContent(atoms, bonds);
    return m.formula();
}

const TemplateLibrary::Fragment *TemplateLibrary::find(const QString &id) const {
    for (const Fragment &f : builtins()) if (f.id == id) return &f;
    for (const Fragment &f : m_mine) if (f.id == id) return &f;
    return nullptr;
}

QVariantList TemplateLibrary::categories() const {
    const auto cat = [](const char *id, const char *name) {
        return QVariantMap{{QStringLiteral("id"), QString::fromLatin1(id)}, {QStringLiteral("name"), QString::fromLatin1(name)}};
    };
    return {cat("projections", "Projections"), cat("amino", "Amino acids"), cat("bases", "Nucleobases"),
            cat("reagents", "Reagents"), cat("mine", "Mine")};
}

QVariantList TemplateLibrary::items(const QString &category) const {
    QVariantList result;
    const auto add = [&](const Fragment &f, bool deletable) {
        result.append(QVariantMap{{QStringLiteral("id"), f.id}, {QStringLiteral("name"), f.name}, {QStringLiteral("label"), f.label},
                                  {QStringLiteral("formula"), f.formula}, {QStringLiteral("category"), f.category},
                                  {QStringLiteral("deletable"), deletable}});
    };
    if (category == QStringLiteral("mine")) {
        for (const Fragment &f : m_mine) add(f, true);
    } else {
        for (const Fragment &f : builtins()) if (f.category == category) add(f, false);
    }
    return result;
}

QVariantMap TemplateLibrary::preview(const QString &id) const {
    const Fragment *f = find(id);
    if (!f) return {};
    QVariantList atoms, bonds;
    QHash<int, int> index;
    for (const Atom &a : f->atoms) {
        index.insert(a.id, atoms.size());
        atoms.append(QVariantMap{{QStringLiteral("x"), a.x}, {QStringLiteral("y"), a.y},
                                 {QStringLiteral("element"), a.element}, {QStringLiteral("charge"), a.formalCharge}});
    }
    for (const Bond &b : f->bonds)
        bonds.append(QVariantMap{{QStringLiteral("a"), index.value(b.atomA)}, {QStringLiteral("b"), index.value(b.atomB)},
                                 {QStringLiteral("order"), b.order}, {QStringLiteral("style"), b.style}});
    return {{QStringLiteral("atoms"), atoms}, {QStringLiteral("bonds"), bonds}};
}

QString TemplateLibrary::formulaOf(const QString &id) const {
    const Fragment *f = find(id);
    return f ? f->formula : QString();
}

QVariantList TemplateLibrary::place(const QString &id, double x, double y) {
    const Fragment *f = find(id);
    if (!f || !m_molecule) return {};
    const QVariantList created = m_molecule->insertFragment(f->atoms, f->bonds, x, y);
    emit message(QStringLiteral("Placed %1.").arg(f->name));
    return created;
}

bool TemplateLibrary::saveSelection(const QVariantList &atomIds) {
    if (!m_molecule) return false;
    QSet<int> wanted;
    for (const QVariant &v : atomIds) wanted.insert(v.toInt());
    Fragment f;
    f.category = QStringLiteral("mine");
    QHash<int, int> newId;
    for (const Atom &a : m_molecule->atomData()) {
        if (!wanted.contains(a.id)) continue;
        Atom copy = a;
        copy.id = f.atoms.size() + 1;
        newId.insert(a.id, copy.id);
        f.atoms.append(copy);
    }
    if (f.atoms.isEmpty()) {
        emit message(QStringLiteral("Select atoms first, then save them as a template."));
        return false;
    }
    int bondId = 1;
    for (Bond b : m_molecule->bondData()) {
        if (!newId.contains(b.atomA) || !newId.contains(b.atomB)) continue;
        b.id = bondId++;
        b.atomA = newId.value(b.atomA);
        b.atomB = newId.value(b.atomB);
        f.bonds.append(b);
    }
    centre(f.atoms);
    f.formula = fragmentFormula(f.atoms, f.bonds);
    QSet<QString> names;
    for (const Fragment &other : std::as_const(m_mine)) names.insert(other.name);
    int counter = 1;
    while (names.contains(QStringLiteral("%1 #%2").arg(f.formula).arg(counter))) ++counter;
    f.name = QStringLiteral("%1 #%2").arg(f.formula).arg(counter);
    f.label = f.formula;
    f.id = QStringLiteral("mine:%1").arg(m_nextMineId++);
    m_mine.append(f);
    if (!writeMine()) {
        m_mine.removeLast();
        --m_nextMineId;
        emit message(QStringLiteral("Could not write %1.").arg(filePath()));
        return false;
    }
    emit mineChanged();
    emit message(QStringLiteral("Saved template %1.").arg(f.name));
    return true;
}

bool TemplateLibrary::deleteMine(const QString &id) {
    for (int i = 0; i < m_mine.size(); ++i) {
        if (m_mine[i].id != id) continue;
        const Fragment removed = m_mine.takeAt(i);
        if (!writeMine()) {
            m_mine.insert(i, removed);
            emit message(QStringLiteral("Could not write %1.").arg(filePath()));
            return false;
        }
        emit mineChanged();
        emit message(QStringLiteral("Deleted template %1.").arg(removed.name));
        return true;
    }
    return false;
}

bool TemplateLibrary::writeMine() {
    if (!QDir().mkpath(m_directory)) return false;
    QJsonArray templates;
    for (const Fragment &f : std::as_const(m_mine)) {
        QJsonArray atoms, bonds;
        QHash<int, int> index;
        for (const Atom &a : f.atoms) { index.insert(a.id, atoms.size()); atoms.append(atomToJson(a)); }
        for (const Bond &b : f.bonds) bonds.append(bondToJson(b, index.value(b.atomA), index.value(b.atomB)));
        templates.append(QJsonObject{{QStringLiteral("id"), f.id.mid(5).toInt()}, {QStringLiteral("name"), f.name},
                                     {QStringLiteral("atoms"), atoms}, {QStringLiteral("bonds"), bonds}});
    }
    const QJsonObject root{{QStringLiteral("format"), QStringLiteral("reChemDrawable templates")}, {QStringLiteral("version"), 1},
                           {QStringLiteral("nextId"), m_nextMineId}, {QStringLiteral("templates"), templates}};
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

void TemplateLibrary::loadMine() {
    m_mine.clear();
    m_nextMineId = 1;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    int maxId = 0;
    for (const QJsonValue &value : root.value(QStringLiteral("templates")).toArray()) {
        const QJsonObject object = value.toObject();
        Fragment f;
        f.category = QStringLiteral("mine");
        const int id = object.value(QStringLiteral("id")).toInt(maxId + 1);
        maxId = qMax(maxId, id);
        f.id = QStringLiteral("mine:%1").arg(id);
        for (const QJsonValue &av : object.value(QStringLiteral("atoms")).toArray()) {
            const QJsonObject a = av.toObject();
            Atom atom;
            atom.id = f.atoms.size() + 1;
            atom.element = a.value(QStringLiteral("element")).toString(QStringLiteral("C"));
            atom.x = a.value(QStringLiteral("x")).toDouble();
            atom.y = a.value(QStringLiteral("y")).toDouble();
            atom.formalCharge = a.value(QStringLiteral("charge")).toInt();
            atom.isotope = a.value(QStringLiteral("isotope")).toInt();
            atom.radical = a.value(QStringLiteral("radical")).toInt();
            atom.lonePairs = a.value(QStringLiteral("lonePairs")).toInt();
            f.atoms.append(atom);
        }
        if (f.atoms.isEmpty()) continue;
        for (const QJsonValue &bv : object.value(QStringLiteral("bonds")).toArray()) {
            const QJsonObject b = bv.toObject();
            const int ia = b.value(QStringLiteral("a")).toInt(-1), ib = b.value(QStringLiteral("b")).toInt(-1);
            if (ia < 0 || ib < 0 || ia >= f.atoms.size() || ib >= f.atoms.size() || ia == ib) continue;
            Bond bond;
            bond.id = f.bonds.size() + 1;
            bond.atomA = ia + 1;
            bond.atomB = ib + 1;
            bond.order = qBound(1, b.value(QStringLiteral("order")).toInt(1), 3);
            bond.style = b.value(QStringLiteral("style")).toString(QStringLiteral("plain"));
            bond.stereo = b.value(QStringLiteral("stereo")).toInt();
            bond.doubleSide = qBound(-1, b.value(QStringLiteral("doubleSide")).toInt(), 1);
            f.bonds.append(bond);
        }
        f.formula = fragmentFormula(f.atoms, f.bonds);
        f.name = object.value(QStringLiteral("name")).toString(f.formula);
        f.label = f.formula;
        m_mine.append(f);
    }
    m_nextMineId = qMax(root.value(QStringLiteral("nextId")).toInt(1), maxId + 1);
}
