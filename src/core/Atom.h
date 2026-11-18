#pragma once

#include <QString>

struct Atom {
    int id = -1;
    QString element = QStringLiteral("C");
    double x = 0.0;
    double y = 0.0;
    int formalCharge = 0;
    int isotope = 0;
    int radical = 0;
};
