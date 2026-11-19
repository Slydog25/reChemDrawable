#include "Abbreviations.h"

#include <QHash>
#include <QVector>

namespace Abbreviations {
namespace {
struct Row {
    const char *name;
    const char *reversed;
    const char *smiles;
    QMap<QString, int> composition;
};

const QVector<Row> &rows() {
    // Composition checked against the SMILES (attachment H removed).
    static const QVector<Row> data = {
        {"Me", "Me", "C", {{"C", 1}, {"H", 3}}},
        {"Et", "Et", "CC", {{"C", 2}, {"H", 5}}},
        {"nPr", "nPr", "CCC", {{"C", 3}, {"H", 7}}},
        {"iPr", "iPr", "C(C)C", {{"C", 3}, {"H", 7}}},
        {"nBu", "nBu", "CCCC", {{"C", 4}, {"H", 9}}},
        {"tBu", "tBu", "C(C)(C)C", {{"C", 4}, {"H", 9}}},
        {"Ph", "Ph", "c%91ccccc%91", {{"C", 6}, {"H", 5}}},
        {"Bn", "Bn", "Cc%91ccccc%91", {{"C", 7}, {"H", 7}}},
        {"PMB", "PMB", "Cc%91ccc(OC)cc%91", {{"C", 8}, {"H", 9}, {"O", 1}}},
        {"Ac", "Ac", "C(C)=O", {{"C", 2}, {"H", 3}, {"O", 1}}},
        {"Bz", "Bz", "C(=O)c%91ccccc%91", {{"C", 7}, {"H", 5}, {"O", 1}}},
        {"Piv", "Piv", "C(=O)C(C)(C)C", {{"C", 5}, {"H", 9}, {"O", 1}}},
        {"Boc", "Boc", "C(=O)OC(C)(C)C", {{"C", 5}, {"H", 9}, {"O", 2}}},
        {"Cbz", "Cbz", "C(=O)OCc%91ccccc%91", {{"C", 8}, {"H", 7}, {"O", 2}}},
        {"Fmoc", "Fmoc", "C(=O)OCC%91c%92ccccc%92-c%92ccccc%92%91", {{"C", 15}, {"H", 11}, {"O", 2}}},
        {"Ts", "Ts", "S(=O)(=O)c%91ccc(C)cc%91", {{"C", 7}, {"H", 7}, {"O", 2}, {"S", 1}}},
        {"Ms", "Ms", "S(C)(=O)=O", {{"C", 1}, {"H", 3}, {"O", 2}, {"S", 1}}},
        {"Tf", "Tf", "S(=O)(=O)C(F)(F)F", {{"C", 1}, {"F", 3}, {"O", 2}, {"S", 1}}},
        {"TMS", "TMS", "[Si](C)(C)C", {{"C", 3}, {"H", 9}, {"Si", 1}}},
        {"TBS", "TBS", "[Si](C)(C)C(C)(C)C", {{"C", 6}, {"H", 15}, {"Si", 1}}},
        {"TIPS", "TIPS", "[Si](C(C)C)(C(C)C)C(C)C", {{"C", 9}, {"H", 21}, {"Si", 1}}},
        {"OMe", "MeO", "OC", {{"C", 1}, {"H", 3}, {"O", 1}}},
        {"OEt", "EtO", "OCC", {{"C", 2}, {"H", 5}, {"O", 1}}},
        {"OAc", "AcO", "OC(C)=O", {{"C", 2}, {"H", 3}, {"O", 2}}},
        {"OTs", "TsO", "OS(=O)(=O)c%91ccc(C)cc%91", {{"C", 7}, {"H", 7}, {"O", 3}, {"S", 1}}},
        {"OTf", "TfO", "OS(=O)(=O)C(F)(F)F", {{"C", 1}, {"F", 3}, {"O", 3}, {"S", 1}}},
        {"OTBS", "TBSO", "O[Si](C)(C)C(C)(C)C", {{"C", 6}, {"H", 15}, {"O", 1}, {"Si", 1}}},
        {"NHBoc", "BocHN", "NC(=O)OC(C)(C)C", {{"C", 5}, {"H", 10}, {"N", 1}, {"O", 2}}},
        {"CF3", "F3C", "C(F)(F)F", {{"C", 1}, {"F", 3}}},
        {"CHO", "OHC", "C=O", {{"C", 1}, {"H", 1}, {"O", 1}}},
        {"CO2H", "HO2C", "C(=O)O", {{"C", 1}, {"H", 1}, {"O", 2}}},
        {"CO2Me", "MeO2C", "C(=O)OC", {{"C", 2}, {"H", 3}, {"O", 2}}},
        {"CO2Et", "EtO2C", "C(=O)OCC", {{"C", 3}, {"H", 5}, {"O", 2}}},
        {"CN", "NC", "C#N", {{"C", 1}, {"N", 1}}},
        {"NO2", "O2N", "[N+](=O)[O-]", {{"N", 1}, {"O", 2}}},
    };
    return data;
}

const QHash<QString, Info> &table() {
    static const QHash<QString, Info> data = [] {
        QHash<QString, Info> result;
        for (const Row &row : rows()) {
            const Info info{QString::fromLatin1(row.name), QString::fromLatin1(row.reversed),
                            QString::fromLatin1(row.smiles), row.composition};
            result.insert(info.name, info);
        }
        return result;
    }();
    return data;
}
}

const Info *find(const QString &name) {
    const auto it = table().constFind(name);
    return it == table().cend() ? nullptr : &it.value();
}

bool isAbbreviation(const QString &name) { return find(name) != nullptr; }

QStringList names() {
    QStringList result;
    for (const Row &row : rows()) result.append(QString::fromLatin1(row.name));
    return result;
}

} // namespace Abbreviations
