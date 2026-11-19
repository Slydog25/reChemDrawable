#include "Properties.h"

#include "Abbreviations.h"
#include "Elements.h"
#include "Molecule.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <queue>

namespace Properties {
namespace {

const double ProtonMass = 1.00727646688;
const double SodiumIonMass = 22.9897692809 - 0.00054857991; // Na+
const int MaxPatternBins = 40;
const double PatternCutoffPercent = 0.01;

// Polar contributions of abbreviation groups attached through their first atom;
// the per-atom environment rules cannot see inside a single drawn label.
struct GroupPolarity {
    const char *name;
    double tpsa;        // N and O
    double tpsaSulfur;  // S
    int donors;
};
const GroupPolarity GroupPolarities[] = {
    {"PMB", 9.23, 0, 0},    {"Ac", 17.07, 0, 0},    {"Bz", 17.07, 0, 0},    {"Piv", 17.07, 0, 0},
    {"Boc", 26.30, 0, 0},   {"Cbz", 26.30, 0, 0},   {"Fmoc", 26.30, 0, 0},  {"Ts", 34.14, 8.38, 0},
    {"Ms", 34.14, 8.38, 0}, {"Tf", 34.14, 8.38, 0}, {"OMe", 9.23, 0, 0},    {"OEt", 9.23, 0, 0},
    {"OAc", 26.30, 0, 0},   {"OTs", 43.37, 8.38, 0}, {"OTf", 43.37, 8.38, 0}, {"OTBS", 9.23, 0, 0},
    {"NHBoc", 38.33, 0, 1}, {"CHO", 17.07, 0, 0},   {"CO2H", 37.30, 0, 1},  {"CO2Me", 26.30, 0, 0},
    {"CO2Et", 26.30, 0, 0}, {"CN", 23.79, 0, 0},    {"NO2", 43.14, 0, 0},
};

// Everything the property rules need to know about one atom in scope.
struct Node {
    const Atom *atom = nullptr;
    const Abbreviations::Info *group = nullptr;
    int hydrogens = 0; // implicit plus explicit H neighbours
    int single = 0, doubleBonds = 0, triple = 0, aromaticBonds = 0; // bonds to non-hydrogen atoms
    bool aromatic = false;
    bool inThreeRing = false;
    bool isHydrogen = false;
    QVector<QPair<int, int>> neighbours; // (node, bond)
};

// Small rings (up to 8 atoms): for every bond, the shortest cycle through it.
QVector<QVector<int>> findRings(const QVector<Node> &nodes, const QVector<QPair<int, int>> &bonds) {
    QVector<QVector<int>> rings;
    QSet<QString> seen;
    for (int e = 0; e < bonds.size(); ++e) {
        const int start = bonds[e].first, goal = bonds[e].second;
        QVector<int> previous(nodes.size(), -2);
        std::queue<int> queue;
        previous[start] = -1;
        queue.push(start);
        while (!queue.empty() && previous[goal] == -2) {
            const int current = queue.front();
            queue.pop();
            for (const auto &edge : nodes[current].neighbours) {
                if (edge.second == e || previous[edge.first] != -2) continue;
                previous[edge.first] = current;
                queue.push(edge.first);
            }
        }
        if (previous[goal] == -2) continue;
        QVector<int> ring;
        for (int at = goal; at != -1; at = previous[at]) ring.append(at);
        if (ring.size() > 8) continue;
        QVector<int> key = ring;
        std::sort(key.begin(), key.end());
        QString text;
        for (int v : key) text += QString::number(v) + ',';
        if (seen.contains(text)) continue;
        seen.insert(text);
        rings.append(ring);
    }
    return rings;
}

// Aromaticity. Atoms on bonds drawn with the aromatic style are aromatic.
// Otherwise a ring is aromatic when every atom is in a ring double bond (one pi
// electron) or is an N/O/S with a lone pair (two electrons, e.g. pyrrole N,
// furan O) and the electron count is 4n+2. A ring carbon with an exocyclic double
// bond breaks it, so the pyrimidinedione ring of caffeine is not aromatic while
// its imidazole ring is; this reproduces the usual PubChem/Cactvs TPSA values.
void perceiveAromaticity(QVector<Node> &nodes, const QVector<QPair<int, int>> &bondAtoms, const QVector<const Bond *> &bonds,
                         const QVector<QVector<int>> &rings) {
    for (int e = 0; e < bonds.size(); ++e) {
        if (bonds[e]->style == QStringLiteral("aromatic")) nodes[bondAtoms[e].first].aromatic = nodes[bondAtoms[e].second].aromatic = true;
    }
    QVector<bool> inAnyRing(nodes.size(), false);
    for (const QVector<int> &ring : rings) for (int v : ring) inAnyRing[v] = true;
    QSet<int> aromaticRingAtoms;
    for (const QVector<int> &ring : rings) {
        int electrons = 0;
        bool ok = true;
        for (int v : ring) {
            const Node &node = nodes[v];
            if (node.group) { ok = false; break; }
            if (node.aromatic) { electrons += 1; continue; }
            bool doubleToRingAtom = false;
            for (const auto &edge : node.neighbours) {
                const int other = edge.first;
                if (bonds[edge.second]->order == 2 && inAnyRing[other]) doubleToRingAtom = true;
            }
            if (doubleToRingAtom) { electrons += 1; continue; }
            const QString &element = node.atom->element;
            const bool lonePairElement = element == QStringLiteral("N") || element == QStringLiteral("O") ||
                                         element == QStringLiteral("S") || element == QStringLiteral("Se");
            if (lonePairElement && node.atom->formalCharge <= 0 && node.doubleBonds == 0 && node.triple == 0) { electrons += 2; continue; }
            if (element == QStringLiteral("C") && node.atom->formalCharge < 0 && node.doubleBonds == 0) { electrons += 2; continue; }
            ok = false;
            break;
        }
        if (ok && electrons >= 6 && (electrons - 2) % 4 == 0) for (int v : ring) aromaticRingAtoms.insert(v);
    }
    for (int v : aromaticRingAtoms) nodes[v].aromatic = true;
}

// Ertl's fragment table (N, O, S, P). A negative result means the environment is
// not in the table.
double nitrogenContribution(const Node &n) {
    const int c = n.atom->formalCharge, h = n.hydrogens;
    const int s = n.single, d = n.doubleBonds, t = n.triple, a = n.aromaticBonds;
    if (n.aromatic) {
        const int heavy = s + d + t + a;
        if (d || t) return -1;
        if (c == 0) return h >= 1 ? 15.79 : heavy == 2 ? 12.89 : heavy == 3 ? 4.93 : -1;
        if (c == 1) return h >= 1 ? 14.14 : heavy == 2 ? 4.10 : heavy == 3 ? 3.88 : -1;
        return -1;
    }
    if (a) return -1;
    if (c == 0) {
        if (h == 0) {
            if (s == 3 && !d && !t) return n.inThreeRing ? 3.01 : 3.24;
            if (s == 1 && d == 1 && !t) return 12.36;
            if (t == 1 && !s && !d) return 23.79;
            if (s == 1 && d == 2 && !t) return 11.68;
            if (d == 1 && t == 1 && !s) return 13.60;
        } else if (h == 1) {
            if (s == 2 && !d && !t) return n.inThreeRing ? 21.94 : 12.03;
            if (d == 1 && !s && !t) return 23.85;
        } else if (h == 2 && s == 1 && !d && !t) {
            return 26.02;
        }
    } else if (c == 1) {
        if (h == 0) {
            if (s == 4 && !d && !t) return 0.0;
            if (s == 2 && d == 1 && !t) return 3.01; // nitro, N-oxides
            if (s == 1 && t == 1 && !d) return 4.36;
        } else if (h == 1) {
            if (s == 3 && !d && !t) return 4.44;
            if (s == 1 && d == 1 && !t) return 13.97;
        } else if (h == 2) {
            if (s == 2 && !d && !t) return 16.61;
            if (d == 1 && !s && !t) return 25.59;
        } else if (h == 3 && s == 1 && !d && !t) {
            return 27.64;
        }
    }
    return -1;
}

double oxygenContribution(const Node &n) {
    const int c = n.atom->formalCharge, h = n.hydrogens;
    const int s = n.single, d = n.doubleBonds, t = n.triple, a = n.aromaticBonds;
    if (t) return -1;
    if (n.aromatic && c == 0 && h == 0 && s + a == 2 && !d) return 13.14;
    if (a) return -1;
    if (c == 0) {
        if (h == 0 && s == 2 && !d) return n.inThreeRing ? 12.53 : 9.23;
        if (h == 0 && d == 1 && !s) return 17.07;
        if (h == 1 && s == 1 && !d) return 20.23;
    } else if (c == -1 && h == 0 && s == 1 && !d) {
        return 23.06;
    }
    return -1;
}

double sulfurContribution(const Node &n) {
    const int c = n.atom->formalCharge, h = n.hydrogens;
    const int s = n.single, d = n.doubleBonds, t = n.triple, a = n.aromaticBonds;
    if (c != 0 || t) return -1;
    if (n.aromatic && h == 0 && s + a == 2 && !d) return 28.24;
    if (a) return -1;
    if (h == 0) {
        if (s == 2 && !d) return 25.30;
        if (s == 0 && d == 1) return 32.09;
        if (s == 2 && d == 1) return 19.21;
        if (s == 2 && d == 2) return 8.38;
    } else if (h == 1 && s == 1 && !d) {
        return 38.80;
    }
    return -1;
}

double phosphorusContribution(const Node &n) {
    const int c = n.atom->formalCharge, h = n.hydrogens;
    const int s = n.single, d = n.doubleBonds, t = n.triple, a = n.aromaticBonds;
    if (c != 0 || t || a) return -1;
    if (h == 0) {
        if (s == 3 && !d) return 13.59;
        if (s == 1 && d == 1) return 34.14;
        if (s == 3 && d == 1) return 9.81;
    } else if (h == 1 && s == 2 && d == 1) {
        return 23.47;
    }
    return -1;
}

struct Distribution {
    QVector<double> probability; // by nominal offset
    QVector<double> massSum;     // probability * exact mass
};

Distribution convolve(const Distribution &a, const Distribution &b) {
    Distribution out;
    const int size = qMin(MaxPatternBins, a.probability.size() + b.probability.size() - 1);
    out.probability.fill(0.0, size);
    out.massSum.fill(0.0, size);
    for (int i = 0; i < a.probability.size(); ++i) {
        if (a.probability[i] <= 0.0) continue;
        for (int j = 0; j < b.probability.size() && i + j < size; ++j) {
            if (b.probability[j] <= 0.0) continue;
            out.probability[i + j] += a.probability[i] * b.probability[j];
            // the mass of a combination is the sum of its parts' masses
            out.massSum[i + j] += a.massSum[i] * b.probability[j] + b.massSum[j] * a.probability[i];
        }
    }
    return out;
}

Distribution power(const Distribution &base, int exponent) {
    Distribution result;
    result.probability = {1.0};
    result.massSum = {0.0};
    Distribution square = base;
    while (exponent > 0) {
        if (exponent & 1) result = convolve(result, square);
        exponent >>= 1;
        if (exponent) square = convolve(square, square);
    }
    return result;
}

QString formatFormula(const QMap<QString, QMap<int, int>> &input, int charge) {
    QMap<QString, QMap<int, int>> counts = input;
    QString result;
    const auto append = [&result, &counts](const QString &element) {
        const QMap<int, int> isotopes = counts.take(element);
        for (auto it = isotopes.cbegin(); it != isotopes.cend(); ++it) {
            if (it.value() <= 0) continue;
            result += it.key() ? QStringLiteral("[%1%2]").arg(it.key()).arg(element) : element;
            if (it.value() > 1) result += QString::number(it.value());
        }
    };
    append(QStringLiteral("C"));
    append(QStringLiteral("H"));
    while (!counts.isEmpty()) append(counts.firstKey());
    if (charge) result += charge == 1 ? QStringLiteral("+") : charge == -1 ? QStringLiteral("-") :
                          charge > 0 ? QStringLiteral("%1+").arg(charge) : QStringLiteral("%1-").arg(-charge);
    return result;
}

} // namespace

Report compute(const Molecule &molecule, const QVector<int> &atomIds) {
    Report report;
    const QVector<Atom> &allAtoms = molecule.atomData();
    const QVector<Bond> &allBonds = molecule.bondData();

    // Atoms in scope: all, or the connected components containing the selection.
    QHash<int, int> indexById;
    for (int i = 0; i < allAtoms.size(); ++i) indexById.insert(allAtoms[i].id, i);
    QVector<bool> inScope(allAtoms.size(), atomIds.isEmpty());
    if (!atomIds.isEmpty()) {
        QVector<QVector<int>> adjacency(allAtoms.size());
        for (const Bond &bond : allBonds) {
            const int a = indexById.value(bond.atomA, -1), b = indexById.value(bond.atomB, -1);
            if (a < 0 || b < 0) continue;
            adjacency[a].append(b);
            adjacency[b].append(a);
        }
        QVector<int> stack;
        for (int id : atomIds) {
            const int index = indexById.value(id, -1);
            if (index >= 0 && !inScope[index]) { inScope[index] = true; stack.append(index); }
        }
        report.selectionOnly = !stack.isEmpty();
        while (!stack.isEmpty()) {
            const int current = stack.takeLast();
            for (int next : adjacency[current]) if (!inScope[next]) { inScope[next] = true; stack.append(next); }
        }
        // A selection that names no atom falls back to the whole drawing.
        if (!report.selectionOnly) inScope.fill(true);
    }

    QVector<Node> nodes;
    QHash<int, int> nodeById;
    for (int i = 0; i < allAtoms.size(); ++i) {
        if (!inScope[i]) continue;
        Node node;
        node.atom = &allAtoms[i];
        node.group = Abbreviations::find(allAtoms[i].element);
        node.isHydrogen = allAtoms[i].element == QStringLiteral("H");
        node.hydrogens = molecule.implicitHydrogens(allAtoms[i]);
        nodeById.insert(allAtoms[i].id, nodes.size());
        nodes.append(node);
    }
    if (nodes.isEmpty()) return report;
    report.atomCount = nodes.size();

    QVector<QPair<int, int>> bondAtoms;
    QVector<const Bond *> bonds;
    for (const Bond &bond : allBonds) {
        const int a = nodeById.value(bond.atomA, -1), b = nodeById.value(bond.atomB, -1);
        if (a < 0 || b < 0) continue;
        const int e = bonds.size();
        bonds.append(&bond);
        bondAtoms.append({a, b});
        nodes[a].neighbours.append({b, e});
        nodes[b].neighbours.append({a, e});
    }
    // Hydrogens drawn as atoms count as hydrogens on their partner, and are not
    // part of its bond environment (the table is written for implicit H).
    const auto classify = [&](int self, int other, const Bond &bond, bool sharedAromaticRing) {
        if (nodes[other].isHydrogen) { nodes[self].hydrogens += 1; return; }
        if (bond.style == QStringLiteral("aromatic") || (sharedAromaticRing && nodes[self].aromatic && nodes[other].aromatic)) nodes[self].aromaticBonds += 1;
        else if (bond.order == 1) nodes[self].single += 1;
        else if (bond.order == 2) nodes[self].doubleBonds += 1;
        else nodes[self].triple += 1;
    };
    for (int e = 0; e < bonds.size(); ++e) {
        classify(bondAtoms[e].first, bondAtoms[e].second, *bonds[e], false);
        classify(bondAtoms[e].second, bondAtoms[e].first, *bonds[e], false);
    }

    const QVector<QVector<int>> rings = findRings(nodes, bondAtoms);
    for (const QVector<int> &ring : rings) if (ring.size() == 3) for (int v : ring) nodes[v].inThreeRing = true;
    perceiveAromaticity(nodes, bondAtoms, bonds, rings);
    // Re-count the bond environment of aromatic atoms: ring bonds shared by two
    // aromatic atoms become ':' (Kekule double and single alike); a biaryl link
    // stays a single bond.
    for (Node &node : nodes) if (node.aromatic) node.single = node.doubleBonds = node.triple = node.aromaticBonds = 0;
    for (int e = 0; e < bonds.size(); ++e) {
        const int a = bondAtoms[e].first, b = bondAtoms[e].second;
        bool sharedRing = false;
        if (nodes[a].aromatic && nodes[b].aromatic) {
            for (const QVector<int> &ring : rings) if (ring.contains(a) && ring.contains(b)) { sharedRing = true; break; }
        }
        for (int side = 0; side < 2; ++side) {
            const int self = side ? b : a, other = side ? a : b;
            if (!nodes[self].aromatic || nodes[other].isHydrogen) continue;
            classify(self, other, *bonds[e], sharedRing);
        }
    }

    // Composition: element -> isotope label (0 = natural) -> count, groups expanded.
    QMap<QString, QMap<int, int>> counts;
    int totalCharge = 0;
    double dou = 1.0;
    QSet<QString> unknown;
    const auto addElement = [&](const QString &symbol, int isotope, int amount) {
        if (amount > 0) counts[symbol][isotope] += amount;
    };
    for (const Node &node : nodes) {
        totalCharge += node.atom->formalCharge;
        if (node.group) {
            for (auto it = node.group->composition.cbegin(); it != node.group->composition.cend(); ++it) addElement(it.key(), 0, it.value());
            continue;
        }
        addElement(node.atom->element, node.atom->isotope, 1);
        if (!node.isHydrogen) addElement(QStringLiteral("H"), 0, molecule.implicitHydrogens(*node.atom));
    }
    report.formula = formatFormula(counts, totalCharge);

    QMap<QString, ElementShare> shares;
    for (auto element = counts.cbegin(); element != counts.cend(); ++element) {
        const Elements::Info *info = Elements::find(element.key());
        if (!info) unknown.insert(element.key());
        for (auto iso = element.value().cbegin(); iso != element.value().cend(); ++iso) {
            const int n = iso.value();
            // A labelled atom contributes its mass number, as in Molecule::properties().
            const double average = iso.key() ? iso.key() : (info ? info->averageMass : 0.0);
            const double mono = iso.key() ? iso.key() : (info ? info->monoisotopicMass : 0.0);
            report.molecularWeight += n * average;
            report.exactMass += n * mono;
            ElementShare &share = shares[element.key()];
            share.symbol = element.key();
            share.count += n;
            share.mass += n * average;
            if (info) dou += n * (info->normalValences.first() - 2) / 2.0;
        }
    }
    report.degreesOfUnsaturation = dou + totalCharge / 2.0;
    const auto takeShare = [&](const QString &symbol) {
        if (!shares.contains(symbol)) return;
        ElementShare share = shares.take(symbol);
        if (report.molecularWeight > 0.0) share.percent = 100.0 * share.mass / report.molecularWeight;
        if (share.mass > 0.0) report.elements.append(share);
    };
    takeShare(QStringLiteral("C"));
    takeShare(QStringLiteral("H"));
    while (!shares.isEmpty()) takeShare(shares.firstKey());

    // Hydrogen bonding and TPSA.
    double tpsaSulfurPhosphorus = 0.0;
    for (const Node &node : nodes) {
        const QString &element = node.atom->element;
        if (node.group) {
            report.hydrogenBondAcceptors += node.group->composition.value(QStringLiteral("N")) + node.group->composition.value(QStringLiteral("O"));
            for (const GroupPolarity &row : GroupPolarities) {
                if (node.group->name != QLatin1String(row.name)) continue;
                report.hydrogenBondDonors += row.donors;
                report.tpsa += row.tpsa;
                tpsaSulfurPhosphorus += row.tpsaSulfur;
            }
        } else if (element == QStringLiteral("N") || element == QStringLiteral("O")) {
            report.hydrogenBondDonors += node.hydrogens;
            report.hydrogenBondAcceptors += 1;
            const double value = element == QStringLiteral("N") ? nitrogenContribution(node) : oxygenContribution(node);
            if (value < 0) report.tpsaUnmatched += 1;
            else report.tpsa += value;
        } else if (element == QStringLiteral("S") || element == QStringLiteral("P")) {
            const double value = element == QStringLiteral("S") ? sulfurContribution(node) : phosphorusContribution(node);
            if (value > 0) tpsaSulfurPhosphorus += value;
        }
    }
    report.tpsaWithSulfurPhosphorus = report.tpsa + tpsaSulfurPhosphorus;
    if (report.molecularWeight > 500.0) report.ruleOfFiveViolations += 1;
    if (report.hydrogenBondDonors > 5) report.ruleOfFiveViolations += 1;
    if (report.hydrogenBondAcceptors > 10) report.ruleOfFiveViolations += 1;

    // Isotope pattern: convolve the per-element isotope distributions.
    Distribution total;
    total.probability = {1.0};
    total.massSum = {0.0};
    int lightestNominal = 0;
    for (auto element = counts.cbegin(); element != counts.cend(); ++element) {
        const QVector<Elements::Isotope> &isotopes = Elements::isotopes(element.key());
        if (isotopes.isEmpty()) { unknown.insert(element.key()); continue; }
        for (auto iso = element.value().cbegin(); iso != element.value().cend(); ++iso) {
            Distribution one;
            int baseMass = 0;
            if (iso.key()) {
                // A labelled atom is a single isotope at its mass number (table mass when known).
                double mass = iso.key();
                for (const Elements::Isotope &candidate : isotopes) if (candidate.massNumber == iso.key()) mass = candidate.mass;
                baseMass = iso.key();
                one.probability = {1.0};
                one.massSum = {mass};
            } else {
                baseMass = isotopes.first().massNumber;
                const int span = isotopes.last().massNumber - baseMass + 1;
                one.probability.fill(0.0, span);
                one.massSum.fill(0.0, span);
                for (const Elements::Isotope &isotope : isotopes) {
                    one.probability[isotope.massNumber - baseMass] = isotope.abundance;
                    one.massSum[isotope.massNumber - baseMass] = isotope.abundance * isotope.mass;
                }
            }
            lightestNominal += baseMass * iso.value();
            total = convolve(total, power(one, iso.value()));
        }
    }
    bool patternComplete = true;
    for (auto element = counts.cbegin(); element != counts.cend(); ++element) if (Elements::isotopes(element.key()).isEmpty()) patternComplete = false;
    double top = 0.0;
    for (double p : total.probability) top = qMax(top, p);
    // No pattern at all beats a misleading one when an element has no isotope data.
    if (top > 0.0 && patternComplete) {
        for (int i = 0; i < total.probability.size(); ++i) {
            const double relative = 100.0 * total.probability[i] / top;
            if (relative < PatternCutoffPercent) continue;
            Peak peak;
            peak.offset = i;
            peak.nominal = lightestNominal + i;
            peak.mass = total.massSum[i] / total.probability[i];
            peak.relative = relative;
            report.pattern.append(peak);
        }
    }

    // Quasi-molecular ions of the monoisotopic mass; the proton and Na+ masses
    // already account for the electron.
    report.adducts = {{QStringLiteral("[M+H]+"), report.exactMass + ProtonMass},
                      {QStringLiteral("[M+Na]+"), report.exactMass + SodiumIonMass},
                      {QStringLiteral("[M−H]−"), report.exactMass - ProtonMass}};
    report.unknownElements = QStringList(unknown.cbegin(), unknown.cend());
    report.unknownElements.sort();
    return report;
}

QVariantMap toVariant(const Report &report) {
    QVariantList elements;
    for (const ElementShare &share : report.elements) {
        elements.append(QVariantMap{{QStringLiteral("symbol"), share.symbol}, {QStringLiteral("count"), share.count},
                                    {QStringLiteral("percent"), share.percent}});
    }
    QVariantList pattern;
    for (const Peak &peak : report.pattern) {
        pattern.append(QVariantMap{{QStringLiteral("label"), peak.offset == 0 ? QStringLiteral("M") : QStringLiteral("M+%1").arg(peak.offset)},
                                   {QStringLiteral("offset"), peak.offset}, {QStringLiteral("nominal"), peak.nominal},
                                   {QStringLiteral("mass"), peak.mass}, {QStringLiteral("relative"), peak.relative}});
    }
    QVariantList adducts;
    for (const Adduct &adduct : report.adducts) {
        adducts.append(QVariantMap{{QStringLiteral("label"), adduct.label}, {QStringLiteral("mz"), adduct.mz}});
    }
    return {{QStringLiteral("selectionOnly"), report.selectionOnly}, {QStringLiteral("atomCount"), report.atomCount},
            {QStringLiteral("formula"), report.formula}, {QStringLiteral("molecularWeight"), report.molecularWeight},
            {QStringLiteral("exactMass"), report.exactMass}, {QStringLiteral("degreesOfUnsaturation"), report.degreesOfUnsaturation},
            {QStringLiteral("hydrogenBondDonors"), report.hydrogenBondDonors},
            {QStringLiteral("hydrogenBondAcceptors"), report.hydrogenBondAcceptors},
            {QStringLiteral("ruleOfFiveViolations"), report.ruleOfFiveViolations},
            {QStringLiteral("tpsa"), report.tpsa}, {QStringLiteral("tpsaWithSulfurPhosphorus"), report.tpsaWithSulfurPhosphorus},
            {QStringLiteral("tpsaUnmatched"), report.tpsaUnmatched},
            {QStringLiteral("elements"), elements}, {QStringLiteral("pattern"), pattern}, {QStringLiteral("adducts"), adducts},
            {QStringLiteral("unknownElements"), report.unknownElements}};
}

} // namespace Properties
