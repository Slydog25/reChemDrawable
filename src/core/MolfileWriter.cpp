#include "MolfileWriter.h"

#include "Abbreviations.h"
#include "GeometryEngine.h"
#include "Molecule.h"

#include <QHash>
#include <QTextStream>

QString MolfileWriter::writeV2000(const Molecule &molecule, const QString &title) {
    QString output;
    QTextStream stream(&output);
    const QVector<Atom> &atoms = molecule.atomData();
    const QVector<Bond> &bonds = molecule.bondData();
    QHash<int, int> indices;
    for (int index = 0; index < atoms.size(); ++index) indices.insert(atoms[index].id, index + 1);

    stream << title.left(80) << "\n  reChemDrawable\n\n";
    stream << QString::number(atoms.size()).rightJustified(3) << QString::number(bonds.size()).rightJustified(3)
           << "  0  0  0  0            999 V2000\n";
    for (const Atom &atom : atoms) {
        stream << QString::number(atom.x / GeometryEngine::StandardBondLength, 'f', 4).rightJustified(10)
               << QString::number(-atom.y / GeometryEngine::StandardBondLength, 'f', 4).rightJustified(10)
               << QString::number(0.0, 'f', 4).rightJustified(10)
               << " " << (Abbreviations::isAbbreviation(atom.element) ? QStringLiteral("R") : atom.element).leftJustified(3, ' ') << " 0  0  0  0  0  0  0  0  0  0  0\n";
    }
    for (const Bond &bond : bonds) {
        const int molfileOrder = bond.style == QStringLiteral("aromatic") ? 4 : bond.order;
        // V2000 bond stereo: 1 = up (wedge), 6 = down (dash), 4 = either (wavy single),
        // 3 = cis/trans either (wavy double).
        int stereo = 0;
        if (bond.style == QStringLiteral("wedge")) stereo = 1;
        else if (bond.style == QStringLiteral("dash")) stereo = 6;
        else if (bond.style == QStringLiteral("wavy")) stereo = bond.order == 2 ? 3 : bond.order == 1 ? 4 : 0;
        stream << QString::number(indices.value(bond.atomA)).rightJustified(3)
               << QString::number(indices.value(bond.atomB)).rightJustified(3)
               << QString::number(molfileOrder).rightJustified(3)
               << QString::number(stereo).rightJustified(3) << "  0  0  0\n";
    }
    for (int index = 0; index < atoms.size(); ++index) {
        // Group abbreviations are "R" atoms with an alias line.
        if (Abbreviations::isAbbreviation(atoms[index].element))
            stream << "A  " << QString::number(index + 1).rightJustified(3) << "\n" << atoms[index].element << "\n";
    }
    for (int index = 0; index < atoms.size(); ++index) {
        const Atom &atom = atoms[index];
        if (atom.radical) stream << "M  RAD  1" << QString::number(index + 1).rightJustified(4)
                                 << QString::number(atom.radical == 1 ? 2 : 3).rightJustified(4) << "\n";
        if (atom.formalCharge) stream << "M  CHG  1" << QString::number(index + 1).rightJustified(4)
                                      << QString::number(atom.formalCharge).rightJustified(4) << "\n";
        if (atom.isotope) stream << "M  ISO  1" << QString::number(index + 1).rightJustified(4)
                                 << QString::number(atom.isotope).rightJustified(4) << "\n";
    }
    stream << "M  END\n";
    return output;
}
