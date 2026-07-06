#include "FloorPlan.h"
#include <numeric>

bool FloorPlan::isValidPoint(const Point& p) const {
    if (!outerBoundary.containsPoint(p)) return false;
    for (const auto& obstacle : obstacles) {
        if (obstacle.containsPoint(p)) return false;
    }
    return true;
}

Polygon::BBox FloorPlan::boundingBox() const {
    return outerBoundary.boundingBox();
}

double FloorPlan::openArea() const {
    double area = outerBoundary.area();
    for (const auto& obs : obstacles)
        area -= obs.area();
    return area;
}
