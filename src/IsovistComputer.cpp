#include "IsovistComputer.h"
#include <cmath>
#include <algorithm>
#include <limits>

// ---- segment collection ----

std::vector<IsovistComputer::Segment>
IsovistComputer::collectSegments(const FloorPlan& fp) const {
    std::vector<Segment> segs;

    auto addPolygonEdges = [&](const Polygon& poly) {
        const auto& verts = poly.vertices();
        size_t n = verts.size();
        for (size_t i = 0; i < n; ++i)
            segs.push_back({verts[i], verts[(i + 1) % n]});
    };

    addPolygonEdges(fp.outerBoundary);
    for (const auto& obs : fp.obstacles)
        addPolygonEdges(obs);

    return segs;
}

// ---- ray-segment intersection ----
//
// Ray:     P + t * (cosA, sinA),  t >= 0
// Segment: A + u * (B - A),       0 <= u <= 1
//
// Derivation (Cramer's rule on [d | -e] * [t; u] = r, where e=B-A, r=A-P):
//   denom = e.x * sinA - e.y * cosA
//   t     = (e.x * (A.y - P.y) - e.y * (A.x - P.x)) / denom
//   u     = (cosA * (A.y - P.y) - sinA * (A.x - P.x)) / denom

double IsovistComputer::raySegmentT(const Point& origin, double cosA, double sinA,
                                     const Segment& seg) const {
    double ex = seg.b.x - seg.a.x;
    double ey = seg.b.y - seg.a.y;

    double denom = ex * sinA - ey * cosA;
    if (std::abs(denom) < 1e-12) return -1.0;

    double rx = seg.a.x - origin.x;
    double ry = seg.a.y - origin.y;

    double t = (ex * ry - ey * rx) / denom;
    double u = (cosA * ry - sinA * rx) / denom;

    if (t < 1e-9 || u < -1e-9 || u > 1.0 + 1e-9) return -1.0;

    return t;
}

// ---- ray cast: returns closest hit across all segments ----

Point IsovistComputer::castRay(const Point& origin, double angle,
                                const std::vector<Segment>& segs) const {
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    double minT = std::numeric_limits<double>::max();

    for (const auto& seg : segs) {
        double t = raySegmentT(origin, cosA, sinA, seg);
        if (t > 0 && t < minT) minT = t;
    }

    // Should always hit the outer boundary; if not, return origin as fallback
    if (minT == std::numeric_limits<double>::max())
        return origin;

    return Point(origin.x + cosA * minT, origin.y + sinA * minT);
}

// ---- main computation ----

Polygon IsovistComputer::compute(const Point& viewpoint, const FloorPlan& floorPlan) const {
    auto segs = collectSegments(floorPlan);

    // Gather all unique angles to every segment endpoint, plus ±ε offsets
    std::vector<double> angles;
    angles.reserve(segs.size() * 6);

    for (const auto& seg : segs) {
        for (const Point* p : {&seg.a, &seg.b}) {
            double a = std::atan2(p->y - viewpoint.y, p->x - viewpoint.x);
            angles.push_back(a - ANGLE_EPS);
            angles.push_back(a);
            angles.push_back(a + ANGLE_EPS);
        }
    }

    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end()), angles.end());

    // Cast one ray per angle; results form the visibility polygon
    Polygon visibility;
    for (double angle : angles)
        visibility.addVertex(castRay(viewpoint, angle, segs));

    return visibility;
}
