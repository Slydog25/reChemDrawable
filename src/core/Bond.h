#pragma once

#include <QString>

struct Bond {
    int id = -1;
    int atomA = -1;
    int atomB = -1;
    int order = 1;
    int stereo = 0;
    QString style = QStringLiteral("plain"); // plain, wedge, dash, wavy, aromatic, benzene
};
