#pragma once

#include <QString>

struct Atom {
    int id = -1;
    QString element = QStringLiteral("C");
    double x = 0.0;
    double y = 0.0;
    int formalCharge = 0;
    int isotope = 0;
    int radical = 0;     // number of radical dots, 0-2
    int lonePairs = 0;   // drawn lone pairs, 0-3 (not stored in Molfiles)
};
