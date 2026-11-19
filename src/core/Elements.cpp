#include "Elements.h"

#include <QHash>
#include <QStringList>
#include <QtGlobal>

namespace Elements {
namespace {
const QHash<QString, Info> &table() {
    static const QHash<QString, Info> data = [] {
        const QVector<Info> rows = {
            {QStringLiteral("H"), {1}, ChargeRule::LoseEither, false, 1.008, 1.007825},
            {QStringLiteral("Li"), {1}, ChargeRule::LoseEither, true, 6.94, 7.016003},
            {QStringLiteral("B"), {3}, ChargeRule::Group13, false, 10.81, 11.009305},
            {QStringLiteral("C"), {4}, ChargeRule::LoseEither, false, 12.011, 12.0},
            {QStringLiteral("N"), {3}, ChargeRule::Group15To17, false, 14.007, 14.003074},
            {QStringLiteral("O"), {2}, ChargeRule::Group15To17, false, 15.999, 15.994915},
            {QStringLiteral("F"), {1}, ChargeRule::Group15To17, false, 18.998, 18.998403},
            {QStringLiteral("Na"), {1}, ChargeRule::LoseEither, true, 22.990, 22.989770},
            {QStringLiteral("Mg"), {2}, ChargeRule::LoseEither, true, 24.305, 23.985042},
            {QStringLiteral("Si"), {4}, ChargeRule::LoseEither, false, 28.085, 27.976927},
            {QStringLiteral("P"), {3, 5}, ChargeRule::Group15To17, false, 30.974, 30.973762},
            {QStringLiteral("S"), {2, 4, 6}, ChargeRule::Group15To17, false, 32.06, 31.972071},
            {QStringLiteral("Cl"), {1}, ChargeRule::Group15To17, false, 35.45, 34.968853},
            {QStringLiteral("K"), {1}, ChargeRule::LoseEither, true, 39.098, 38.963707},
            {QStringLiteral("Se"), {2, 4, 6}, ChargeRule::Group15To17, false, 78.971, 79.916522},
            {QStringLiteral("Br"), {1}, ChargeRule::Group15To17, false, 79.904, 78.918338},
            {QStringLiteral("I"), {1}, ChargeRule::Group15To17, false, 126.904, 126.904468},
        };
        QHash<QString, Info> result;
        for (const Info &row : rows) result.insert(row.symbol, row);
        return result;
    }();
    return data;
}

int shifted(const Info &info, int valence, int charge) {
    switch (info.rule) {
    case ChargeRule::LoseEither: return valence - qAbs(charge);
    case ChargeRule::Group13: return valence - charge;
    case ChargeRule::Group15To17: return valence + charge;
    }
    return valence;
}
}

const Info *find(const QString &symbol) {
    const auto it = table().constFind(symbol);
    return it == table().cend() ? nullptr : &it.value();
}

int maxValence(const QString &symbol, int charge) {
    const Info *info = find(symbol);
    if (!info) return 4;
    return qMax(0, shifted(*info, info->normalValences.last(), charge));
}

int implicitHydrogens(const QString &symbol, int charge, int bondOrderSum) {
    const Info *info = find(symbol);
    if (!info || info->metal || symbol == QStringLiteral("H")) return 0;
    for (const int valence : info->normalValences) {
        const int adjusted = shifted(*info, valence, charge);
        if (adjusted >= bondOrderSum) return adjusted - bondOrderSum;
    }
    return 0;
}

bool isOrganicSubset(const QString &symbol) {
    static const QVector<QString> subset = {QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("N"),
        QStringLiteral("O"), QStringLiteral("P"), QStringLiteral("S"), QStringLiteral("F"),
        QStringLiteral("Cl"), QStringLiteral("Br"), QStringLiteral("I")};
    return subset.contains(symbol);
}

bool canBeAromatic(const QString &symbol) {
    static const QVector<QString> aromatic = {QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("N"),
        QStringLiteral("O"), QStringLiteral("P"), QStringLiteral("S"), QStringLiteral("Se")};
    return aromatic.contains(symbol);
}

int atomicNumber(const QString &symbol) {
    static const QStringList symbols = QStringList()
        << "H" << "He" << "Li" << "Be" << "B" << "C" << "N" << "O" << "F" << "Ne"
        << "Na" << "Mg" << "Al" << "Si" << "P" << "S" << "Cl" << "Ar" << "K" << "Ca"
        << "Sc" << "Ti" << "V" << "Cr" << "Mn" << "Fe" << "Co" << "Ni" << "Cu" << "Zn"
        << "Ga" << "Ge" << "As" << "Se" << "Br" << "Kr" << "Rb" << "Sr" << "Y" << "Zr"
        << "Nb" << "Mo" << "Tc" << "Ru" << "Rh" << "Pd" << "Ag" << "Cd" << "In" << "Sn"
        << "Sb" << "Te" << "I" << "Xe" << "Cs" << "Ba" << "La" << "Ce" << "Pr" << "Nd"
        << "Pm" << "Sm" << "Eu" << "Gd" << "Tb" << "Dy" << "Ho" << "Er" << "Tm" << "Yb"
        << "Lu" << "Hf" << "Ta" << "W" << "Re" << "Os" << "Ir" << "Pt" << "Au" << "Hg"
        << "Tl" << "Pb" << "Bi" << "Po" << "At" << "Rn" << "Fr" << "Ra" << "Ac" << "Th"
        << "Pa" << "U" << "Np" << "Pu" << "Am" << "Cm" << "Bk" << "Cf" << "Es" << "Fm"
        << "Md" << "No" << "Lr" << "Rf" << "Db" << "Sg" << "Bh" << "Hs" << "Mt" << "Ds"
        << "Rg" << "Cn" << "Nh" << "Fl" << "Mc" << "Lv" << "Ts" << "Og";
    const int index = symbols.indexOf(symbol);
    return index < 0 ? 0 : index + 1;
}

} // namespace Elements
