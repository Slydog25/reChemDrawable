#include "GeometryEngine.h"

#include <QtMath>

namespace {
constexpr double Pi = 3.14159265358979323846;
}

QPointF GeometryEngine::snapBondEnd(const QPointF &origin, const QPointF &target,
                                    int incrementDegrees) {
    const QPointF delta = target - origin;
    const double length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    if (length < 1.0 || incrementDegrees <= 0) {
        return origin + QPointF(StandardBondLength, 0.0);
    }

    const double increment = qDegreesToRadians(static_cast<double>(incrementDegrees));
    const double snappedAngle = qRound(qAtan2(delta.y(), delta.x()) / increment) * increment;
    return origin + QPointF(qCos(snappedAngle) * StandardBondLength,
                            qSin(snappedAngle) * StandardBondLength);
}

QVector<QPointF> GeometryEngine::regularPolygon(const QPointF &center, int sides,
                                                 double radius, double rotationDegrees) {
    QVector<QPointF> points;
    if (sides < 3) {
        return points;
    }
    if (radius <= 0.0) {
        radius = StandardBondLength / (2.0 * qSin(Pi / sides));
    }
    const double rotation = qDegreesToRadians(rotationDegrees);
    for (int index = 0; index < sides; ++index) {
        const double angle = rotation + (2.0 * Pi * index / sides);
        points.append(center + QPointF(qCos(angle) * radius, qSin(angle) * radius));
    }
    return points;
}
