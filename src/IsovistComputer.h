#pragma once
#include "Point.h"
#include "Polygon.h"
#include "FloorPlan.h"
#include <vector>

class IsovistComputer {
public:
    // Computes the 2D visibility polygon from viewpoint within floorPlan.
    Polygon compute(const Point& viewpoint, const FloorPlan& floorPlan) const;

private:
    struct Segment { Point a, b; };

    std::vector<Segment> collectSegments(const FloorPlan& fp) const;

    // Returns t along ray (origin + t*(cosA,sinA)) to nearest intersection with seg,
    // or -1 if none.
    double raySegmentT(const Point& origin, double cosA, double sinA,
                       const Segment& seg) const;

    // Cast ray at given angle, return nearest hit point across all segments.
    Point castRay(const Point& origin, double angle,
                  const std::vector<Segment>& segs) const;

    static constexpr double ANGLE_EPS = 1e-6;
};
