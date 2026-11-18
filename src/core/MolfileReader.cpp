#include "MolfileReader.h"

#include "GeometryEngine.h"
#include "Molecule.h"

#include <QStringList>

bool MolfileReader::readV2000(const QString &contents, Molecule *molecule, QString *error) {
    if (!molecule) return false;
    const QStringList lines = contents.split('\n');
    if (lines.size() < 4) { if (error) *error = QStringLiteral("File is too short for a V2000 Molfile."); return false; }
    bool atomsOk = false, bondsOk = false;
    const int atomCount = lines[3].mid(0, 3).trimmed().toInt(&atomsOk);
    const int bondCount = lines[3].mid(3, 3).trimmed().toInt(&bondsOk);
    if (!atomsOk || !bondsOk || lines.size() < 4 + atomCount + bondCount) { if (error) *error = QStringLiteral("Invalid V2000 counts line."); return false; }
    QVector<Atom> atoms;
    QVector<Bond> bonds;
    for (int index = 0; index < atomCount; ++index) {
        const QString line = lines[4 + index];
        bool xOk = false, yOk = false;
        const double x = line.mid(0, 10).trimmed().toDouble(&xOk);
        const double y = line.mid(10, 10).trimmed().toDouble(&yOk);
        const QString element = line.mid(31, 3).trimmed();
        if (!xOk || !yOk || element.isEmpty()) { if (error) *error = QStringLiteral("Invalid atom record."); return false; }
        atoms.append({index + 1, element, x * GeometryEngine::StandardBondLength, -y * GeometryEngine::StandardBondLength});
    }
    for (int index = 0; index < bondCount; ++index) {
        const QString line = lines[4 + atomCount + index];
        bool aOk = false, bOk = false, orderOk = false;
        const int a = line.mid(0, 3).trimmed().toInt(&aOk);
        const int b = line.mid(3, 3).trimmed().toInt(&bOk);
        const int rawOrder = line.mid(6, 3).trimmed().toInt(&orderOk);
        bool stereoOk = false;
        const int stereo = line.mid(9, 3).trimmed().toInt(&stereoOk);
        if (!aOk || !bOk || !orderOk || a < 1 || a > atomCount || b < 1 || b > atomCount || rawOrder < 1 || rawOrder > 4) { if (error) *error = QStringLiteral("Invalid bond record."); return false; }
        const QString style = rawOrder == 4 ? QStringLiteral("aromatic") : stereo == 1 ? QStringLiteral("wedge") : stereo == 6 ? QStringLiteral("dash") : QStringLiteral("plain");
        bonds.append({index + 1, a, b, rawOrder == 4 ? 1 : rawOrder, stereoOk ? stereo : 0, style});
    }
    for (int lineIndex = 4 + atomCount + bondCount; lineIndex < lines.size(); ++lineIndex) {
        const QString line = lines[lineIndex];
        if (line.startsWith(QStringLiteral("M  END"))) break;
        if (!line.startsWith(QStringLiteral("M  CHG")) && !line.startsWith(QStringLiteral("M  ISO"))) continue;
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 4) continue;
        bool countOk = false;
        const int pairCount = parts[2].toInt(&countOk);
        if (!countOk || parts.size() < 3 + pairCount * 2) continue;
        for (int pair = 0; pair < pairCount; ++pair) {
            bool atomOk = false, valueOk = false;
            const int atomIndex = parts[3 + pair * 2].toInt(&atomOk) - 1;
            const int value = parts[4 + pair * 2].toInt(&valueOk);
            if (!atomOk || !valueOk || atomIndex < 0 || atomIndex >= atoms.size()) continue;
            if (line.startsWith(QStringLiteral("M  CHG"))) atoms[atomIndex].formalCharge = value;
            else atoms[atomIndex].isotope = value;
        }
    }
    molecule->replaceContent(atoms, bonds);
    return true;
}
