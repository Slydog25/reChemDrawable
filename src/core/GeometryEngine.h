#pragma once

#include <QPointF>
#include <QVector>

class GeometryEngine {
public:
    static constexpr double StandardBondLength = 48.0;

    static QPointF snapBondEnd(const QPointF &origin, const QPointF &target,
                               int incrementDegrees = 30);
    static QVector<QPointF> regularPolygon(const QPointF &center, int sides,
                                           double radius = -1.0,
                                           double rotationDegrees = -90.0);
};
